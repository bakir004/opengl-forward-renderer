#include "terrain/TerrainVegetation.h"

#include "assets/AssetImporter.h"
#include "core/ShaderProgram.h"
#include "core/Texture2D.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <unordered_map>

// ─── SpatialGrid ─────────────────────────────────────────────────────────────
// Accelerates minimum-spacing rejection.  Divides the XZ plane into cells of
// size cellSize; an insert + lookup is O(1) amortised.

struct TerrainVegetation::SpatialGrid
{
    float cellSize;

    explicit SpatialGrid(float cs) : cellSize(cs) {}

    bool IsTooClose(glm::vec2 xz, float minDist) const
    {
        const int cx   = static_cast<int>(std::floor(xz.x / cellSize));
        const int cz   = static_cast<int>(std::floor(xz.y / cellSize));
        const int range = static_cast<int>(std::ceil(minDist / cellSize)) + 1;
        const float d2 = minDist * minDist;

        for (int dx = -range; dx <= range; ++dx)
            for (int dz = -range; dz <= range; ++dz)
            {
                auto it = m_cells.find(Key(cx + dx, cz + dz));
                if (it == m_cells.end()) continue;
                for (const glm::vec2& p : it->second)
                {
                    glm::vec2 d = xz - p;
                    if (glm::dot(d, d) < d2) return true;
                }
            }
        return false;
    }

    void Insert(glm::vec2 xz)
    {
        const int cx = static_cast<int>(std::floor(xz.x / cellSize));
        const int cz = static_cast<int>(std::floor(xz.y / cellSize));
        m_cells[Key(cx, cz)].push_back(xz);
    }

private:
    static uint64_t Key(int cx, int cz)
    {
        return (static_cast<uint64_t>(cx + 100000)) * 200001u
             +  static_cast<uint64_t>(cz + 100000);
    }
    std::unordered_map<uint64_t, std::vector<glm::vec2>> m_cells;
};

// ─── Heightfield sampling helpers ────────────────────────────────────────────

static glm::vec2 GridToWorld(uint32_t gx, uint32_t gz,
                              const TerrainHeightfield& hf)
{
    return {
        (static_cast<float>(gx) / static_cast<float>(hf.width  - 1) - 0.5f) * hf.settings.worldWidth,
        (static_cast<float>(gz) / static_cast<float>(hf.height - 1) - 0.5f) * hf.settings.worldHeight
    };
}

static float SampleHeightAt(const TerrainHeightfield& hf, glm::vec2 xz)
{
    const float W  = static_cast<float>(hf.width  - 1);
    const float H  = static_cast<float>(hf.height - 1);
    const float ww = hf.settings.worldWidth;
    const float wh = hf.settings.worldHeight;

    float u = glm::clamp(xz.x / ww + 0.5f, 0.0f, 1.0f);
    float v = glm::clamp(xz.y / wh + 0.5f, 0.0f, 1.0f);

    float fx = u * W, fz = v * H;
    auto  x0 = static_cast<uint32_t>(fx);
    auto  z0 = static_cast<uint32_t>(fz);
    uint32_t x1 = std::min(x0 + 1, hf.width  - 1);
    uint32_t z1 = std::min(z0 + 1, hf.height - 1);
    float tx = fx - static_cast<float>(x0);
    float tz = fz - static_cast<float>(z0);

    return glm::mix(
        glm::mix(hf.At(x0, z0).height, hf.At(x1, z0).height, tx),
        glm::mix(hf.At(x0, z1).height, hf.At(x1, z1).height, tx),
        tz);
}

// Compute per-instance scale modulation from local terrain metrics.
// worldScaleFactor  — global world-size multiplier (computed once in PlaceAll)
// s                 — terrain sample at the instance position
// treelineNorm      — normalised height above which vegetation shrinks
static float ComputeAutoScale(float worldScaleFactor,
                               const TerrainSample& s,
                               float treelineNorm = 0.72f)
{
    // Elevation: full size below treeline, taper to 60% above it
    const float elevFactor = (s.normalizedHeight < treelineNorm)
        ? 1.0f
        : 1.0f - (s.normalizedHeight - treelineNorm) / (1.0f - treelineNorm) * 0.40f;

    // Slope: steep ground → smaller props (roots can't anchor)
    const float slopeFactor = 1.0f - glm::clamp(s.slope * 0.45f, 0.0f, 0.50f);

    // Roughness via erosion amount: heavily eroded patches → slightly smaller
    const float roughFactor = 1.0f - glm::clamp(s.erosionAmount * 0.30f, 0.0f, 0.35f);

    return worldScaleFactor * elevFactor * slopeFactor * roughFactor;
}

// ─── Frustum helpers ─────────────────────────────────────────────────────────

static void ExtractFrustumPlanes(const glm::mat4& vp, glm::vec4 planes[6])
{
    // Gribb-Hartmann method — extract rows from GLM column-major matrix
    glm::vec4 row0(vp[0][0], vp[1][0], vp[2][0], vp[3][0]);
    glm::vec4 row1(vp[0][1], vp[1][1], vp[2][1], vp[3][1]);
    glm::vec4 row2(vp[0][2], vp[1][2], vp[2][2], vp[3][2]);
    glm::vec4 row3(vp[0][3], vp[1][3], vp[2][3], vp[3][3]);
    planes[0] = row3 + row0;  planes[1] = row3 - row0;  // left, right
    planes[2] = row3 + row1;  planes[3] = row3 - row1;  // bottom, top
    planes[4] = row3 + row2;  planes[5] = row3 - row2;  // near, far
    for (int i = 0; i < 6; ++i)
    {
        float len = glm::length(glm::vec3(planes[i]));
        if (len > 0.0001f) planes[i] /= len;
    }
}

static bool SphereInFrustum(const glm::vec4 planes[6],
                             glm::vec3 center, float radius)
{
    for (int i = 0; i < 6; ++i)
        if (glm::dot(glm::vec3(planes[i]), center) + planes[i].w < -radius)
            return false;
    return true;
}

// ─── Transform builder ───────────────────────────────────────────────────────

static glm::mat4 BuildTransform(glm::vec3 pos, float scale,
                                 float yRotDeg, float tiltXDeg, float tiltZDeg)
{
    glm::mat4 m = glm::translate(glm::mat4(1.0f), pos);
    m = glm::rotate(m, glm::radians(yRotDeg),  glm::vec3(0, 1, 0));
    m = glm::rotate(m, glm::radians(tiltXDeg), glm::vec3(1, 0, 0));
    m = glm::rotate(m, glm::radians(tiltZDeg), glm::vec3(0, 0, 1));
    return glm::scale(m, glm::vec3(scale));
}

// ─── VegetationGroup ─────────────────────────────────────────────────────────

VegetationGroup::~VegetationGroup()
{
    if (m_ssbo != 0)
    {
        glDeleteBuffers(1, &m_ssbo);
        m_ssbo = 0;
    }
}

bool VegetationGroup::Load(const std::string& path,
                            const std::shared_ptr<ShaderProgram>& shader)
{
    m_shader = shader;
    m_model  = AssetImporter::LoadModel(path);
    if (!m_model.IsValid())
    {
        spdlog::warn("[VegetationGroup] Failed to load: {}", path);
        return false;
    }

    auto whiteFallback = std::make_shared<Texture2D>(
        Texture2D::CreateFallback(200, 200, 200, 255));

    m_baseMat = std::make_shared<Material>(shader);
    m_baseMat->SetVec4("u_TintColor", {1.0f, 1.0f, 1.0f, 1.0f});

    for (const auto& matInfo : m_model.materials)
    {
        auto inst = std::make_unique<MaterialInstance>(m_baseMat);
        inst->SetName(matInfo.name);

        std::shared_ptr<Texture2D> diffuse;
        if (!matInfo.diffusePath.empty())
            diffuse = AssetImporter::LoadTexture(matInfo.diffusePath,
                                                  TextureColorSpace::sRGB);
        inst->SetTexture(TextureSlot::Albedo, diffuse ? diffuse : whiteFallback);

        if (!matInfo.normalPath.empty())
            if (auto n = AssetImporter::LoadTexture(matInfo.normalPath,
                                                      TextureColorSpace::Linear))
                inst->SetTexture(TextureSlot::Normal, n);

        if (!matInfo.metallicRoughnessPath.empty())
            if (auto mr = AssetImporter::LoadTexture(matInfo.metallicRoughnessPath,
                                                       TextureColorSpace::Linear))
            {
                inst->SetTexture(TextureSlot::Metallic,  mr);
                inst->SetTexture(TextureSlot::Roughness, mr);
            }

        if (!matInfo.emissivePath.empty())
            if (auto em = AssetImporter::LoadTexture(matInfo.emissivePath,
                                                       TextureColorSpace::sRGB))
                inst->SetTexture(TextureSlot::Emissive, em);

        inst->SetVec3 ("u_AlbedoColor",       matInfo.albedoColor);
        inst->SetFloat("u_MetallicValue",      matInfo.metallicValue);
        inst->SetFloat("u_RoughnessValue",     matInfo.roughnessValue);
        inst->SetVec3 ("u_EmissiveColor",      matInfo.emissiveColor);
        inst->SetFloat("u_NormalScale",        matInfo.normalScale);
        inst->SetBool ("u_IsSpecularGlossiness", matInfo.isSpecularGlossiness);
        if (matInfo.isSpecularGlossiness)
        {
            inst->SetVec3 ("u_SpecularFactor",   matInfo.specularFactor);
            inst->SetFloat("u_GlossinessFactor", matInfo.glossinessFactor);
        }

        m_matInstances.push_back(std::move(inst));
    }

    spdlog::info("[VegetationGroup '{}'] Loaded: {} submesh(es), {} material(s)",
                 name, m_model.mesh->SubMeshCount(), m_model.materials.size());
    return true;
}

void VegetationGroup::AddInstance(glm::vec3 worldPos, float scale,
                                   float yRotDeg, float tiltXDeg, float tiltZDeg)
{
    VegetationInstance inst;
    inst.position      = worldPos;
    inst.boundingRadius = scale * radiusFactor;
    inst.transform     = BuildTransform(worldPos, scale * defaultScale,
                                         yRotDeg, tiltXDeg, tiltZDeg);
    m_instances.push_back(inst);
}

void VegetationGroup::ClearInstances()
{
    m_instances.clear();
    m_visibleTransforms.clear();
    m_visibleCount = 0;
}

void VegetationGroup::UploadInstances()
{
    if (m_visibleTransforms.empty()) { m_visibleCount = 0; return; }

    const auto count     = static_cast<uint32_t>(m_visibleTransforms.size());
    const auto byteSize  = static_cast<GLsizeiptr>(count * sizeof(glm::mat4));

    if (m_ssbo == 0) glGenBuffers(1, &m_ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ssbo);

    if (count > m_ssboCapacity)
    {
        m_ssboCapacity = count + count / 2 + 32;
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                     static_cast<GLsizeiptr>(m_ssboCapacity * sizeof(glm::mat4)),
                     nullptr, GL_DYNAMIC_DRAW);
    }
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, byteSize,
                    m_visibleTransforms.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    m_visibleCount = count;
}

void VegetationGroup::Draw() const
{
    if (!IsValid() || m_visibleCount == 0 || m_ssbo == 0) return;

    // Bind shader once; set frame-constant uniforms that the renderer doesn't
    // set on this shader (IBL / shadow disabled for vegetation).
    m_shader->Bind();
    m_shader->SetUniform("u_ReceiveShadow",     0);
    m_shader->SetUniform("u_HasIBL",            false);
    m_shader->SetUniform("u_HasIrradianceMap",  false);
    m_shader->SetUniform("u_HasPrefilteredMap", false);
    m_shader->SetUniform("u_HasBRDFLUT",        false);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_ssbo);

    const uint32_t subCount = m_model.mesh->SubMeshCount();
    for (uint32_t sub = 0; sub < subCount; ++sub)
    {
        const SubMesh& sm = m_model.mesh->GetSubMesh(sub);
        MaterialInstance* mat = nullptr;
        if (sm.materialIndex < m_matInstances.size())
            mat = m_matInstances[sm.materialIndex].get();
        else if (!m_matInstances.empty())
            mat = m_matInstances[0].get();

        // mat->Bind() re-binds the shader and sets material params; uniform
        // values we set above are preserved in the program object.
        if (mat) mat->Bind();
        m_model.mesh->DrawSubMeshInstanced(sub, m_visibleCount);
    }

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, 0);
    ShaderProgram::Unbind();
}

// ─── TerrainVegetation — Setup ────────────────────────────────────────────────

bool TerrainVegetation::Setup(const std::shared_ptr<ShaderProgram>& shader)
{
    if (!shader || !shader->IsValid())
    {
        spdlog::error("[TerrainVegetation] Instanced shader invalid — vegetation disabled");
        return false;
    }

    const std::string base = "assets/models/gltf/terrainGen/";

    auto load = [&](VegetationGroup& g, const char* rel, const char* n,
                    float scale, float radius, float lod) -> bool
    {
        g.name         = n;
        g.defaultScale = scale;
        g.radiusFactor = radius;
        g.lodDistance  = lod;
        return g.Load(base + rel + "/scene.gltf", shader);
    };

    // Forest
    load(m_realisticTree, "forest/realistic_tree",  "realistic_tree",   1.0f, 12.0f, 400.0f);
    load(m_treesLowPoly,  "forest/trees_low_poly",  "trees_low_poly",   0.02f,  9.0f, 350.0f);
    load(m_treeGn,        "forest/tree_gn",          "tree_gn",          1.0f,  9.0f, 320.0f);
    load(m_stylizedLog,   "forest/stylized_log",     "stylized_log",     2.0f,  4.0f, 200.0f);
    load(m_simpleRock,    "forest/a_simple_rock",    "a_simple_rock",    1.0f,  2.5f, 120.0f);

    // Grass
    load(m_lowpolyGrass, "grass/lowpoly_grass",        "lowpoly_grass",   1.0f,  2.0f,  80.0f);
    load(m_flower,       "grass/flower",                "flower",           0.03f,  1.5f, 500.0f);
    load(m_bush,         "grass/photorealistic_bush",   "bush",             1.0f,  3.0f, 120.0f);

    // Rock
    load(m_boulder, "rock/realistic_boulder", "realistic_boulder", 1.0f, 5.0f, 250.0f);
    load(m_rock,    "rock/rock",               "rock",              0.05f, 3.0f, 180.0f);
    load(m_stone,   "rock/stone",              "stone",             0.01f, 2.0f, 130.0f);
    load(m_pebbles, "rock/ground_pebbles",     "ground_pebbles",    1.0f, 2.0f,  70.0f);

    // Sand
    load(m_cactus,  "sand/cactus",                 "cactus",   1.0f, 4.0f, 250.0f);
    load(m_cactus2, "sand/cactus_2_downloadable",  "cactus_2", 1.0f, 4.0f, 250.0f);

    m_setupDone = true;
    spdlog::info("[TerrainVegetation] Setup complete");
    return true;
}

// ─── TerrainVegetation — ClearAll ────────────────────────────────────────────

void TerrainVegetation::ClearAll()
{
    for (VegetationGroup* g : {
            &m_realisticTree, &m_treesLowPoly, &m_treeGn,
            &m_stylizedLog,   &m_simpleRock,
            &m_lowpolyGrass,  &m_flower,       &m_bush,
            &m_boulder,       &m_rock,          &m_stone,  &m_pebbles,
            &m_cactus,        &m_cactus2 })
        g->ClearInstances();

    m_statTrees = m_statGrass = m_statRock = m_statSand = 0;
}

// ─── TerrainVegetation — PlaceAll ────────────────────────────────────────────

void TerrainVegetation::PlaceAll(const TerrainHeightfield& hf, uint32_t seed)
{
    if (!m_setupDone || !m_enabled) return;

    // Derive global scale factor from world size so vegetation always looks
    // proportional regardless of how worldWidth/heightScale are configured.
    // Reference: a 512-unit-wide world at 80-unit heightScale == scale 1.0.
    constexpr float kRefWorldSize  = 512.0f;
    constexpr float kRefHeightScale = 80.0f;
    m_worldScaleFactor = m_autoScale
        ? (hf.settings.worldWidth / kRefWorldSize)
          * std::sqrt(hf.settings.heightScale / kRefHeightScale)
        : 1.0f;

    std::mt19937 rng(seed ^ 0xABCD1234u);

    // Scale spatial-grid cell sizes proportionally so spacing rules remain
    // correct in world units when the world is larger or smaller.
    const float gs = m_worldScaleFactor;
    SpatialGrid treeGrid(7.0f * gs), logGrid(5.0f * gs), rockUndGrid(2.5f * gs);
    SpatialGrid grassGrid(1.5f * gs), flowerGrid(2.0f * gs), bushGrid(3.5f * gs);
    SpatialGrid boulderGrid(6.0f * gs), rockGrid(3.5f * gs), stoneGrid(2.0f * gs), pebblesGrid(1.0f * gs);
    SpatialGrid cactusGrid(9.0f * gs), sandStoneGrid(3.5f * gs);

    PlaceForestZone(hf, rng, treeGrid, logGrid, rockUndGrid);
    PlaceGrassZone (hf, rng, grassGrid, flowerGrid, bushGrid);
    PlaceRockZone  (hf, rng, boulderGrid, rockGrid, stoneGrid, pebblesGrid);
    PlaceSandZone  (hf, rng, cactusGrid, sandStoneGrid);

    spdlog::info("[TerrainVegetation] Placed — trees:{} grass:{} rock:{} sand:{} (worldScale:{:.2f})",
                 m_statTrees, m_statGrass, m_statRock, m_statSand, m_worldScaleFactor);
}

// ─── ScatterCluster ──────────────────────────────────────────────────────────

void TerrainVegetation::ScatterCluster(VegetationGroup& group,
                                        glm::vec3 parentPos, int count,
                                        float radius,
                                        float scaleMin, float scaleMax,
                                        float minSpacing,
                                        std::mt19937& rng, SpatialGrid& grid,
                                        const TerrainHeightfield& hf,
                                        float baseScaleMult)
{
    std::uniform_real_distribution<float> angleDist(0.0f, glm::two_pi<float>());
    std::uniform_real_distribution<float> distDist(0.0f, radius);
    std::uniform_real_distribution<float> scaleDist(scaleMin, scaleMax);
    std::uniform_real_distribution<float> rotDist(0.0f, 360.0f);
    std::uniform_real_distribution<float> tiltDist(-3.0f, 3.0f);

    for (int i = 0; i < count; ++i)
    {
        float   angle = angleDist(rng);
        float   dist  = distDist(rng);
        glm::vec2 xz(parentPos.x + std::cos(angle) * dist,
                     parentPos.z + std::sin(angle) * dist);

        if (grid.IsTooClose(xz, minSpacing)) continue;

        float wy = SampleHeightAt(hf, xz);
        group.AddInstance({xz.x, wy, xz.y},
                          scaleDist(rng) * baseScaleMult, rotDist(rng),
                          tiltDist(rng), tiltDist(rng));
        grid.Insert(xz);
    }
}

// ─── PlaceForestZone ─────────────────────────────────────────────────────────

void TerrainVegetation::PlaceForestZone(const TerrainHeightfield& hf,
                                         std::mt19937& rng,
                                         SpatialGrid& treeGrid,
                                         SpatialGrid& logGrid,
                                         SpatialGrid& rockUndGrid)
{
    const int stride = std::max(1, static_cast<int>(12.0f / m_density));

    std::uniform_real_distribution<float> prob(0.0f, 1.0f);
    std::uniform_int_distribution<int>    clusterN(3, 6);
    std::uniform_real_distribution<float> scaleDist(0.8f, 1.2f);
    std::uniform_real_distribution<float> rotDist(0.0f, 360.0f);
    std::uniform_real_distribution<float> tiltDist(-3.0f, 3.0f);

    for (uint32_t z = 0; z < hf.height; z += stride)
    for (uint32_t x = 0; x < hf.width;  x += stride)
    {
        const TerrainSample& s = hf.At(x, z);
        if (s.treeMask < m_treeMaskMin || s.steepSlopeExclusion < 0.5f) continue;
        if (prob(rng) > s.treeMask) continue;

        glm::vec2 parentXZ = GridToWorld(x, z, hf);
        glm::vec3 parentPos(parentXZ.x, s.height, parentXZ.y);

        const float autoScale = m_autoScale
            ? ComputeAutoScale(m_worldScaleFactor, s, 0.72f)
            : 1.0f;

        // Choose tree species
        float r = prob(rng);
        VegetationGroup* treeGroup =
            (r < 0.55f) ? &m_realisticTree :
            (r < 0.80f) ? &m_treesLowPoly  : &m_treeGn;

        if (!treeGrid.IsTooClose(parentXZ, 6.5f))
        {
            int n = clusterN(rng);
            ScatterCluster(*treeGroup, parentPos, n, 9.0f,
                           0.8f, 1.2f, 6.5f, rng, treeGrid, hf, autoScale);
            m_statTrees += n;
        }

        // Understory: stylized_log (20%)
        if (prob(rng) < 0.20f && !logGrid.IsTooClose(parentXZ, 4.5f))
        {
            float sc = scaleDist(rng) * autoScale;
            float wy = SampleHeightAt(hf, parentXZ + glm::vec2(3.0f, 2.0f));
            m_stylizedLog.AddInstance(
                {parentXZ.x + 3.0f, wy, parentXZ.y + 2.0f},
                sc, rotDist(rng), tiltDist(rng), tiltDist(rng));
            logGrid.Insert(parentXZ + glm::vec2(3.0f, 2.0f));
        }

        // Understory: small rock (15%)
        if (prob(rng) < 0.15f && !rockUndGrid.IsTooClose(parentXZ, 2.0f))
        {
            float sc = scaleDist(rng) * 0.7f * autoScale;
            m_simpleRock.AddInstance(parentPos, sc, rotDist(rng), 0.0f, 0.0f);
            rockUndGrid.Insert(parentXZ);
        }
    }
}

// ─── PlaceGrassZone ──────────────────────────────────────────────────────────

void TerrainVegetation::PlaceGrassZone(const TerrainHeightfield& hf,
                                        std::mt19937& rng,
                                        SpatialGrid& grassGrid,
                                        SpatialGrid& flowerGrid,
                                        SpatialGrid& bushGrid)
{
    const int stride = std::max(1, static_cast<int>(5.0f / m_density));

    std::uniform_real_distribution<float> prob(0.0f, 1.0f);
    std::uniform_int_distribution<int>    clumpN(1, 3);
    std::uniform_real_distribution<float> scaleDist(0.7f, 1.3f);
    std::uniform_real_distribution<float> rotDist(0.0f, 360.0f);
    std::uniform_real_distribution<float> tiltDist(-2.0f, 2.0f);

    for (uint32_t z = 0; z < hf.height; z += stride)
    for (uint32_t x = 0; x < hf.width;  x += stride)
    {
        const TerrainSample& s = hf.At(x, z);
        if (s.grassMask < m_grassMaskMin) continue;
        if (s.treeMask > 0.25f) continue;   // grass suppressed under canopy
        if (s.steepSlopeExclusion < 0.8f)  continue;
        if (prob(rng) > s.grassMask * 0.85f) continue;

        glm::vec2 xz = GridToWorld(x, z, hf);
        glm::vec3 pos(xz.x, s.height, xz.y);

        const float autoScale = m_autoScale
            ? ComputeAutoScale(m_worldScaleFactor, s, 0.80f)
            : 1.0f;

        // Grass clumps (80%)
        if (prob(rng) < 0.80f)
            ScatterCluster(m_lowpolyGrass, pos, clumpN(rng), 2.5f,
                           0.7f, 1.3f, 1.4f, rng, grassGrid, hf, autoScale);

        // Flower (40%)
        if (prob(rng) < 0.40f && !flowerGrid.IsTooClose(xz, 1.8f))
        {
            m_flower.AddInstance(pos, scaleDist(rng) * autoScale, rotDist(rng),
                                 tiltDist(rng), tiltDist(rng));
            flowerGrid.Insert(xz);
            ++m_statGrass;
        }

        // Bush on grass edges (suitability 0.3..0.55): 12%
        if (s.grassMask < 0.55f && prob(rng) < 0.12f
            && !bushGrid.IsTooClose(xz, 3.0f))
        {
            m_bush.AddInstance(pos, scaleDist(rng) * 0.9f * autoScale, rotDist(rng),
                               tiltDist(rng), tiltDist(rng));
            bushGrid.Insert(xz);
            ++m_statGrass;
        }
    }
}

// ─── PlaceRockZone ───────────────────────────────────────────────────────────

void TerrainVegetation::PlaceRockZone(const TerrainHeightfield& hf,
                                       std::mt19937& rng,
                                       SpatialGrid& boulderGrid,
                                       SpatialGrid& rockGrid,
                                       SpatialGrid& stoneGrid,
                                       SpatialGrid& pebblesGrid)
{
    const int stride = std::max(1, static_cast<int>(8.0f / m_density));

    std::uniform_real_distribution<float> prob(0.0f, 1.0f);
    std::uniform_real_distribution<float> scaleDist(0.6f, 1.4f);
    std::uniform_real_distribution<float> rotDist(0.0f, 360.0f);
    std::uniform_real_distribution<float> tiltDist(-5.0f, 5.0f);

    for (uint32_t z = 0; z < hf.height; z += stride)
    for (uint32_t x = 0; x < hf.width;  x += stride)
    {
        const TerrainSample& s = hf.At(x, z);
        if (s.rockMask < m_rockMaskMin) continue;
        if (prob(rng) > s.rockMask * 0.60f) continue;

        glm::vec2 xz = GridToWorld(x, z, hf);
        glm::vec3 pos(xz.x, s.height, xz.y);

        // Rocks don't grow smaller at altitude like trees do, but local slope
        // and world size still apply — use a high treeline so elevation barely matters.
        const float autoScale = m_autoScale
            ? ComputeAutoScale(m_worldScaleFactor, s, 0.95f)
            : 1.0f;

        // Boulder or rock (50/50), with stone companions
        if (prob(rng) < 0.50f && !boulderGrid.IsTooClose(xz, 5.5f))
        {
            float sc = scaleDist(rng) * autoScale;
            m_boulder.AddInstance(pos, sc, rotDist(rng), tiltDist(rng), tiltDist(rng));
            boulderGrid.Insert(xz);
            ++m_statRock;

            ScatterCluster(m_stone, pos, 2, 4.0f, 0.4f, 0.9f, 1.8f, rng, stoneGrid, hf, autoScale);
        }
        else if (!rockGrid.IsTooClose(xz, 3.0f))
        {
            m_rock.AddInstance(pos, scaleDist(rng) * autoScale, rotDist(rng),
                               tiltDist(rng), tiltDist(rng));
            rockGrid.Insert(xz);
            ++m_statRock;
        }

        // Pebble fill (70%)
        if (prob(rng) < 0.70f)
            ScatterCluster(m_pebbles, pos, 2, 3.5f, 0.8f, 1.2f, 0.9f, rng, pebblesGrid, hf, autoScale);
    }
}

// ─── PlaceSandZone ───────────────────────────────────────────────────────────

void TerrainVegetation::PlaceSandZone(const TerrainHeightfield& hf,
                                       std::mt19937& rng,
                                       SpatialGrid& cactusGrid,
                                       SpatialGrid& sandStoneGrid)
{
    const int stride = std::max(1, static_cast<int>(12.0f / m_density));

    std::uniform_real_distribution<float> prob(0.0f, 1.0f);
    std::uniform_real_distribution<float> scaleDist(0.8f, 1.2f);
    std::uniform_real_distribution<float> rotDist(0.0f, 360.0f);

    for (uint32_t z = 0; z < hf.height; z += stride)
    for (uint32_t x = 0; x < hf.width;  x += stride)
    {
        const TerrainSample& s = hf.At(x, z);
        if (s.materialZone == TerrainMaterialZone::DeepWater ||
            s.materialZone == TerrainMaterialZone::ShallowWater) continue;
        if (s.normalizedHeight > m_sandHeightMax) continue;
        if (s.grassMask > 0.12f || s.treeMask > 0.05f) continue;
        if (s.steepSlopeExclusion < 0.7f) continue;

        glm::vec2 xz = GridToWorld(x, z, hf);
        glm::vec3 pos(xz.x, s.height, xz.y);

        const float autoScale = m_autoScale
            ? ComputeAutoScale(m_worldScaleFactor, s, 0.95f)
            : 1.0f;

        // Cactus (15%)
        if (prob(rng) < 0.15f && !cactusGrid.IsTooClose(xz, 8.0f))
        {
            VegetationGroup& cact = (prob(rng) < 0.55f) ? m_cactus : m_cactus2;
            cact.AddInstance(pos, scaleDist(rng) * autoScale, rotDist(rng), 0.0f, 0.0f);
            cactusGrid.Insert(xz);
            ++m_statSand;
        }

        // Stone scatter (8%)
        if (prob(rng) < 0.08f && !sandStoneGrid.IsTooClose(xz, 3.0f))
        {
            m_stone.AddInstance(pos, scaleDist(rng) * 0.6f * autoScale, rotDist(rng), 0.0f, 0.0f);
            sandStoneGrid.Insert(xz);
        }
    }
}

// ─── TerrainVegetation — CullAndUpload ───────────────────────────────────────

void TerrainVegetation::CullAndUpload(const glm::mat4& viewProj,
                                       const glm::vec3& cameraPos,
                                       float verticalOffset)
{
    if (!m_setupDone || !m_enabled) return;

    const glm::vec3 offsetVec{0.0f, verticalOffset, 0.0f};

    glm::vec4 planes[6];
    ExtractFrustumPlanes(viewProj, planes);

    int visible = 0;
    for (VegetationGroup* g : {
            &m_realisticTree, &m_treesLowPoly, &m_treeGn,
            &m_stylizedLog,   &m_simpleRock,
            &m_lowpolyGrass,  &m_flower,       &m_bush,
            &m_boulder,       &m_rock,          &m_stone,  &m_pebbles,
            &m_cactus,        &m_cactus2 })
    {
        if (!g->IsValid()) continue;

        const float lodDist = g->lodDistance * m_lodDistanceMult;
        g->m_visibleTransforms.clear();

        const bool needsRescale = (g->scaleOverride != 1.0f);
        for (const VegetationInstance& inst : g->m_instances)
        {
            const glm::vec3 shiftedPosition = inst.position + offsetVec;
            float d = glm::length(shiftedPosition - cameraPos);
            if (d > lodDist) continue;
            if (!SphereInFrustum(planes, shiftedPosition, inst.boundingRadius)) continue;

            glm::mat4 transform = glm::translate(glm::mat4(1.0f), offsetVec) * inst.transform;
            if (needsRescale)
                transform = glm::scale(transform, glm::vec3(g->scaleOverride));
            g->m_visibleTransforms.push_back(transform);
        }

        g->UploadInstances();
        visible += static_cast<int>(g->m_visibleCount);
    }
    m_statVisible = visible;
}

// ─── TerrainVegetation — DrawAll ─────────────────────────────────────────────

void TerrainVegetation::DrawAll() const
{
    if (!m_setupDone || !m_enabled) return;

    // Restore depth state (post-process may have disabled it)
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    for (const VegetationGroup* g : {
            &m_realisticTree, &m_treesLowPoly, &m_treeGn,
            &m_stylizedLog,   &m_simpleRock,
            &m_lowpolyGrass,  &m_flower,       &m_bush,
            &m_boulder,       &m_rock,          &m_stone,  &m_pebbles,
            &m_cactus,        &m_cactus2 })
        g->Draw();
}

// ─── TerrainVegetation — OnImGui ─────────────────────────────────────────────

void TerrainVegetation::OnImGui()
{
    ImGui::SeparatorText("Vegetation");

    ImGui::Text("Trees: %d   Grass: %d   Rock: %d   Sand: %d",
                m_statTrees, m_statGrass, m_statRock, m_statSand);
    ImGui::Text("Visible instances: %d", m_statVisible);

    ImGui::Spacing();
    ImGui::Checkbox("Enabled##veg", &m_enabled);
    ImGui::DragFloat("Density##veg",        &m_density,         0.05f, 0.1f,   2.0f, "%.2f");
    ImGui::DragFloat("LOD Scale##veg",       &m_lodDistanceMult, 0.05f, 0.1f,   3.0f, "%.2f");
    ImGui::DragFloat("Tree Mask Min##veg",   &m_treeMaskMin,     0.01f, 0.0f,   1.0f);
    ImGui::DragFloat("Grass Mask Min##veg",  &m_grassMaskMin,    0.01f, 0.0f,   1.0f);
    ImGui::DragFloat("Rock Mask Min##veg",   &m_rockMaskMin,     0.01f, 0.0f,   1.0f);
    ImGui::DragFloat("Sand H Max##veg",      &m_sandHeightMax,   0.005f, 0.0f,  0.3f, "%.3f");

    // ── Per-group scale inspector ─────────────────────────────────────────────
    // scaleOverride is applied live each frame in CullAndUpload — no regen.
    // When a model looks right, press "Bake" to fold the override into
    // defaultScale so placement produces correct sizes on the next Regenerate.
    ImGui::Spacing();
    ImGui::SeparatorText("Per-Model Scale");
    ImGui::TextDisabled("Drag Override to resize live. Bake folds it into Default.");
    ImGui::Spacing();

    // Column headers
    ImGui::Text("%-22s  %7s  %8s  %6s  %6s", "Model", "Count", "Default", "Override", "");
    ImGui::Separator();

    auto groupRow = [](VegetationGroup& g)
    {
        if (!g.IsValid()) return;

        // Name + instance count
        ImGui::Text("%-22s  %7zu", g.name.c_str(), g.InstanceCount());
        ImGui::SameLine();

        // defaultScale — read-only display (changes only on Bake)
        ImGui::Text("  %7.4f", g.defaultScale);
        ImGui::SameLine();

        // scaleOverride — live drag, Ctrl+click for exact value
        char overrideLbl[64];
        snprintf(overrideLbl, sizeof(overrideLbl), "##so_%s", g.name.c_str());
        ImGui::SetNextItemWidth(90.0f);
        ImGui::DragFloat(overrideLbl, &g.scaleOverride, 0.005f, 0.0001f, 50.0f, "%.4f");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Drag or Ctrl+click. Range 0.0001..50.\nShift+drag for coarse.");

        ImGui::SameLine();

        // Bake button — folds override into defaultScale, resets override to 1
        char bakeLbl[64];
        snprintf(bakeLbl, sizeof(bakeLbl), "Bake##bk_%s", g.name.c_str());
        if (ImGui::SmallButton(bakeLbl))
        {
            g.defaultScale *= g.scaleOverride;
            g.scaleOverride = 1.0f;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Bake: default *= override, reset override = 1.\nApply takes effect on next Regenerate.");
    };

    if (ImGui::CollapsingHeader("Forest##vegHdr", ImGuiTreeNodeFlags_DefaultOpen))
    {
        groupRow(m_realisticTree);
        groupRow(m_treesLowPoly);
        groupRow(m_treeGn);
        groupRow(m_stylizedLog);
        groupRow(m_simpleRock);
    }
    if (ImGui::CollapsingHeader("Grass##vegHdr"))
    {
        groupRow(m_lowpolyGrass);
        groupRow(m_flower);
        groupRow(m_bush);
    }
    if (ImGui::CollapsingHeader("Rock##vegHdr"))
    {
        groupRow(m_boulder);
        groupRow(m_rock);
        groupRow(m_stone);
        groupRow(m_pebbles);
    }
    if (ImGui::CollapsingHeader("Sand##vegHdr"))
    {
        groupRow(m_cactus);
        groupRow(m_cactus2);
    }

    ImGui::Spacing();
    if (ImGui::Button("Reset All Overrides##veg"))
        for (VegetationGroup* g : { &m_realisticTree, &m_treesLowPoly, &m_treeGn,
                                     &m_stylizedLog,   &m_simpleRock,
                                     &m_lowpolyGrass,  &m_flower,      &m_bush,
                                     &m_boulder,        &m_rock,        &m_stone,
                                     &m_pebbles,        &m_cactus,      &m_cactus2 })
            g->scaleOverride = 1.0f;

    ImGui::Spacing();
    ImGui::SeparatorText("LOD Distances");
    auto lodRow = [](VegetationGroup& g)
    {
        if (!g.IsValid()) return;
        char lbl[64];
        snprintf(lbl, sizeof(lbl), "%s##lod", g.name.c_str());
        ImGui::SetNextItemWidth(100.0f);
        ImGui::DragFloat(lbl, &g.lodDistance, 5.0f, 20.0f, 1000.0f, "%.0f m");
    };
    lodRow(m_realisticTree); lodRow(m_treesLowPoly); lodRow(m_treeGn);
    lodRow(m_stylizedLog);   lodRow(m_simpleRock);
    lodRow(m_lowpolyGrass);  lodRow(m_flower);        lodRow(m_bush);
    lodRow(m_boulder);       lodRow(m_rock);           lodRow(m_stone);
    lodRow(m_pebbles);       lodRow(m_cactus);         lodRow(m_cactus2);
}
