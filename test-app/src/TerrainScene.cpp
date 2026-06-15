#include "TerrainScene.h"

#include "assets/AssetImporter.h"
#include "core/Camera.h"
#include "core/IInputProvider.h"
#include "core/Material.h"
#include "core/Primitives.h"
#include "core/ShaderProgram.h"
#include "scene/LightBuilder.h"
#include "scene/RenderItem.h"

#include "ui/UITheme.h"
#include <imgui.h>
#include <cstdarg>
#include <spdlog/spdlog.h>
#include <glm/glm.hpp>
#include <chrono>

// GLFW pulls in windows.h which defines GetObject as GetObjectA/W.
// Undefine after all headers so Scene::GetObject resolves correctly.
#undef GetObject

// ─── Zone PBR properties ─────────────────────────────────────────────────────
// Albedo comes from the shader's TerrainPalette(); only roughness/metallic
// are uploaded as uniforms now.

static const float kZoneRoughness[7] = {
    0.20f,  // DeepWater  — reflective water surface
    0.35f,  // ShallowWater
    0.80f,  // Sand
    0.90f,  // Grass
    0.92f,  // Forest
    0.85f,  // Rock
    0.55f,  // Snow       — partially specular ice/snow
};

static const float kZoneMetallic[7] = {
    0.05f,  // DeepWater  — slight metallic sheen
    0.02f,  // ShallowWater
    0.00f,  // Sand
    0.00f,  // Grass
    0.00f,  // Forest
    0.00f,  // Rock
    0.00f,  // Snow
};

// ─────────────────────────────────────────────────────────────────────────────

bool TerrainScene::Setup()
{
    spdlog::info("[TerrainScene] Setting up");
    SetSceneName("Terrain");

    // ── Shader ───────────────────────────────────────────────────────────────
    m_terrainShader = AssetImporter::LoadShader(
        "assets/shaders/terrain.vert",
        "assets/shaders/terrain.frag");

    if (!m_terrainShader || !m_terrainShader->IsValid())
    {
        spdlog::error("[TerrainScene] Failed to load terrain shader");
        return false;
    }

    m_waterShader = AssetImporter::LoadShader(
        "assets/shaders/basic.vert",
        "assets/shaders/basic.frag");
    if (!m_waterShader || !m_waterShader->IsValid())
    {
        spdlog::error("[TerrainScene] Failed to load water shader");
        return false;
    }

    // ── Material ─────────────────────────────────────────────────────────────
    m_terrainMaterial = std::make_shared<Material>(m_terrainShader);
    m_terrainInstance = std::make_unique<MaterialInstance>(m_terrainMaterial);

    // ── Camera ───────────────────────────────────────────────────────────────
    Camera cam;
    cam.SetPosition({0.0f, 120.0f, 200.0f});
    cam.SetOrientation(-90.0f, -25.0f);
    SetCamera(cam);
    m_cameraFreeFlySpeed = 40.0f;

    // ── Sky / clear ───────────────────────────────────────────────────────────
    SetClearColor({0.45f, 0.65f, 0.88f, 1.0f});
    SetSkyboxVisible(false);

    // ── Lights ───────────────────────────────────────────────────────────────
    SetAmbientLight({0.45f, 0.52f, 0.62f}, 0.25f);

    auto& lights = GetLights();
    lights.SetDirectionalLight(
        DirectionalLightBuilder()
            .Direction({-0.5f, -1.0f, -0.4f})
            .Color({1.0f, 0.97f, 0.88f})
            .Intensity(4.0f)
            .CastShadow(true)
            .ShadowResolution(2048, 2048)
            .Name("Sun")
            .Build());

    // ── Instanced vegetation ──────────────────────────────────────────────────
    auto instancedShader = AssetImporter::LoadShader(
        "assets/shaders/instanced.vert",
        "assets/shaders/mesh.frag");
    if (!m_vegetation.Setup(instancedShader))
        spdlog::warn("[TerrainScene] Vegetation setup failed — proceeding without it");

    // ── Water plane ──────────────────────────────────────────────────────────
    CreateWaterPlane();

    // ── Generate terrain + upload ─────────────────────────────────────────────
    Regenerate();

    spdlog::info("[TerrainScene] Setup complete");
    return true;
}

void TerrainScene::Regenerate()
{
    // ── Generation ────────────────────────────────────────────────────────────
    auto t0 = std::chrono::high_resolution_clock::now();
    if (m_genSettings.useHeightmap)
        m_heightfield = TerrainGenerator::LoadHeightmapFromPNG(
            m_genSettings.heightmapPath, m_genSettings, m_classSettings);
    else
        m_heightfield = TerrainGenerator::GenerateHeightfield(m_genSettings, m_classSettings);
    auto t1 = std::chrono::high_resolution_clock::now();
    m_stats.generationTimeMs =
        std::chrono::duration<float, std::milli>(t1 - t0).count();

    // ── Mesh build ────────────────────────────────────────────────────────────
    t0 = std::chrono::high_resolution_clock::now();
    m_meshData = TerrainMeshBuilder::BuildMeshData(m_heightfield);
    t1 = std::chrono::high_resolution_clock::now();
    m_stats.meshBuildTimeMs =
        std::chrono::duration<float, std::milli>(t1 - t0).count();

    m_stats.vertexCount   = m_meshData.VertexCount();
    m_stats.indexCount    = m_meshData.IndexCount();
    m_stats.triangleCount = m_meshData.IndexCount() / 3;
    m_stats.lastSeedStr   = std::to_string(m_genSettings.seed);

    // ── GPU upload ────────────────────────────────────────────────────────────
    UploadToGpu();

    // ── Instanced vegetation ──────────────────────────────────────────────────
    m_vegetation.ClearAll();
    m_vegetation.PlaceAll(m_heightfield, m_genSettings.seed);
}

void TerrainScene::CreateWaterPlane()
{
    PrimitiveMeshData waterData = GenerateQuad({
        .colorMode = ColorMode::Solid,
        .baseColor = {0.02f, 0.32f, 0.55f},
        .doubleSided = true,
    });
    m_waterBuffer = std::make_unique<MeshBuffer>(waterData.CreateMeshBuffer());

    RenderItem item;
    item.mesh = m_waterBuffer.get();
    item.shader = m_waterShader.get();
    item.flags.visible = m_showWaterPlane;
    item.flags.castShadow = false;
    item.flags.receiveShadow = false;
    const float waterY = m_classSettings.shallowWaterHeight * m_genSettings.heightScale;
    item.transform.SetTranslation({0.0f, waterY, 0.0f});
    item.transform.SetRotationEulerDegrees({90.0f, 0.0f, 0.0f});
    item.transform.SetScale({m_genSettings.worldWidth, m_genSettings.worldHeight, 1.0f});

    m_waterObjectIndex = AddObject(item);
}

void TerrainScene::UploadToGpu()
{
    auto t0 = std::chrono::high_resolution_clock::now();
    m_terrainBuffer = TerrainMeshBuilder::UploadMeshData(m_meshData);
    auto t1 = std::chrono::high_resolution_clock::now();
    m_stats.meshUploadTimeMs =
        std::chrono::duration<float, std::milli>(t1 - t0).count();

    if (!m_terrainBuffer)
    {
        spdlog::error("[TerrainScene] Failed to upload terrain mesh to GPU");
        return;
    }

    RenderItem item;
    item.mesh     = m_terrainBuffer.get();
    item.material = m_terrainInstance.get();
    item.flags.castShadow    = true;
    item.flags.receiveShadow = true;
    item.transform.SetTranslation({0.0f, m_terrainVerticalOffset, 0.0f});

    if (!m_terrainAdded)
    {
        m_terrainObjectIndex = AddObject(item);
        m_terrainAdded = true;
    }
    else
    {
        RenderItem& existing = GetObject(m_terrainObjectIndex);
        existing.mesh = m_terrainBuffer.get();
        ApplyTerrainOffset();
    }
}

void TerrainScene::ApplyTerrainOffset()
{
    if (!m_terrainAdded)
        return;

    RenderItem& terrain = GetObject(m_terrainObjectIndex);
    terrain.transform.SetTranslation({0.0f, m_terrainVerticalOffset, 0.0f});

    RenderItem& water = GetObject(m_waterObjectIndex);
    const float waterY = m_classSettings.shallowWaterHeight * m_genSettings.heightScale;
    water.flags.visible = m_showWaterPlane;
    water.transform.SetTranslation({0.0f, waterY, 0.0f});
    water.transform.SetScale({m_genSettings.worldWidth, m_genSettings.worldHeight, 1.0f});
}

void TerrainScene::ApplyMaterialUniforms() const
{
    if (!m_terrainShader || !m_terrainShader->IsValid())
        return;

    m_terrainShader->Bind();

    for (int i = 0; i < 7; ++i)
    {
        m_terrainShader->SetUniform("u_ZoneRoughness[" + std::to_string(i) + "]", kZoneRoughness[i]);
        m_terrainShader->SetUniform("u_ZoneMetallic[" + std::to_string(i) + "]",  kZoneMetallic[i]);
    }

    m_terrainShader->SetUniform("u_DebugView",   static_cast<int>(m_debugView));
    m_terrainShader->SetUniform("u_HeightScale",  m_genSettings.heightScale);
    m_terrainShader->SetUniform("u_FogDensity",   m_fogDensity);
    m_terrainShader->SetUniform("u_FogColor",     m_fogColor);

    ShaderProgram::Unbind();
}

void TerrainScene::OnUpdate(float deltaTime, IInputProvider& input)
{
    UpdateStandardCameraAndPlayer(deltaTime, input, m_playerPos, m_moveDirXZ);
    ApplyMaterialUniforms();

    if (m_needsRegenerate)
    {
        m_needsRegenerate = false;
        Regenerate();
    }

    ApplyTerrainOffset();

    // Per-frame frustum cull + SSBO upload for all vegetation groups.
    const Camera& cam = GetCamera();
    m_vegetation.CullAndUpload(cam.GetViewProjection(), cam.GetPosition(),
                               m_terrainVerticalOffset);
}

void TerrainScene::OnPostRender()
{
    m_vegetation.DrawAll();
}

void TerrainScene::OnTerrainTabUI()
{
    // ── Stats ─────────────────────────────────────────────────────────────────
    if (SectionHeader("Stats"))
    {
        auto Row = [](const char* lbl, const char* fmt, ...) {
            ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextDim);
            ImGui::TextUnformatted(lbl);
            ImGui::PopStyleColor();
            ImGui::SameLine(100);
            va_list ap; va_start(ap, fmt);
            ImGui::TextV(fmt, ap);
            va_end(ap);
        };
        ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextMid);
        Row("Seed",       "%s",       m_stats.lastSeedStr.c_str());
        Row("Vertices",   "%u",       m_stats.vertexCount);
        Row("Triangles",  "%u",       m_stats.triangleCount);
        Row("Gen time",   "%.1f ms",  m_stats.generationTimeMs);
        Row("Build time", "%.1f ms",  m_stats.meshBuildTimeMs);
        Row("Upload",     "%.1f ms",  m_stats.meshUploadTimeMs);
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    // ── Debug view ───────────────────────────────────────────────────────────
    if (SectionHeader("Debug View", false))
    {
        static const char* kViewNames[] = {
            "Off", "Heightmap", "Slope", "Normals",
            "Material Zone", "Mountain Mask", "Grass Mask", "Tree Mask", "Rock Mask"
        };
        int viewIdx = static_cast<int>(m_debugView);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##view", &viewIdx, kViewNames, IM_ARRAYSIZE(kViewNames)))
            m_debugView = static_cast<TerrainDebugView>(viewIdx);
        ImGui::Spacing();
    }

    bool dirty = false;

    // ── Atmosphere ───────────────────────────────────────────────────────────
    if (SectionHeader("Atmosphere"))
    {
        ImGui::DragFloat("Fog Density##atm", &m_fogDensity, 0.0001f, 0.0f, 0.02f, "%.4f");
        ImGui::ColorEdit3("Fog Color##atm",  &m_fogColor.x);
        ImGui::Spacing();
    }

    // ── Placement ────────────────────────────────────────────────────────────
    if (SectionHeader("Placement"))
    {
        ImGui::SliderFloat("Vert Offset##terrainY", &m_terrainVerticalOffset,
                           -m_genSettings.heightScale, m_genSettings.heightScale, "%.1f");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Move the terrain up/down without regenerating. Lower it to turn peaks into islands.");
        ImGui::SameLine();
        if (ImGui::SmallButton("Reset##terrainY"))
            m_terrainVerticalOffset = 0.0f;

        ImGui::Checkbox("Sea Level Water##water", &m_showWaterPlane);
        dirty |= ImGui::SliderFloat("Water Level##wl", &m_classSettings.shallowWaterHeight, 0.0f, 1.0f, "%.3f");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Normalised height for the water-colour transition. The water quad moves immediately; terrain colours need regen.");
        ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextFaint);
        ImGui::Text("  World Y: %.1f", m_classSettings.shallowWaterHeight * m_genSettings.heightScale);
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    // ── Heightmap input ──────────────────────────────────────────────────────
    if (SectionHeader("Heightmap", false))
    {
        dirty |= ImGui::Checkbox("Use PNG Heightmap##hm", &m_genSettings.useHeightmap);
        if (m_genSettings.useHeightmap)
        {
            static char s_pathBuf[512] = {};
            if (s_pathBuf[0] == '\0')
                strncpy(s_pathBuf, m_genSettings.heightmapPath.c_str(), sizeof(s_pathBuf) - 1);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##hmpath", s_pathBuf, sizeof(s_pathBuf)))
            {
                m_genSettings.heightmapPath = s_pathBuf;
                dirty = true;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Relative or absolute path to a PNG or 16-bit heightmap.");
            dirty |= ImGui::DragFloat("Gamma##hm",         &m_genSettings.heightmapGamma,       0.01f, 0.1f, 4.0f, "%.2f");
            dirty |= ImGui::Checkbox ("Flip Y##hm",        &m_genSettings.heightmapFlipY);
            dirty |= ImGui::DragInt  ("Smooth Passes##hm", &m_genSettings.heightmapSmoothPasses, 1, 0, 32);
            dirty |= ImGui::DragInt  ("Blur Radius##hm",   &m_genSettings.heightmapBlurRadius,   1, 1, 64);
            dirty |= ImGui::SliderFloat("Blur Strength##hm", &m_genSettings.heightmapBlurStrength, 0.0f, 1.0f, "%.2f");
        }
        ImGui::Spacing();
    }

    // ── Generation ───────────────────────────────────────────────────────────
    if (SectionHeader("Generation"))
    {
        dirty |= ImGui::DragInt("Seed",    reinterpret_cast<int*>(&m_genSettings.seed));
        dirty |= ImGui::DragInt("Grid W",  reinterpret_cast<int*>(&m_genSettings.gridWidth),  1.0f, 32, 512);
        dirty |= ImGui::DragInt("Grid H",  reinterpret_cast<int*>(&m_genSettings.gridHeight), 1.0f, 32, 512);
        dirty |= ImGui::DragFloat("World W",       &m_genSettings.worldWidth,   1.0f, 64.0f, 2048.0f);
        dirty |= ImGui::DragFloat("World H (Z)",   &m_genSettings.worldHeight,  1.0f, 64.0f, 2048.0f);
        dirty |= ImGui::DragFloat("Height Scale",  &m_genSettings.heightScale,  0.5f, 10.0f, 500.0f);
        dirty |= ImGui::DragInt  ("Smooth Passes", &m_genSettings.heightSmoothingPasses, 1, 0, 10);
        dirty |= ImGui::Checkbox ("Median Smooth", &m_genSettings.heightSmoothingMedian);
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextFaint);
        ImGui::TextUnformatted("Gaussian pre-smooth (before smooth passes)");
        ImGui::PopStyleColor();
        dirty |= ImGui::DragInt  ("Blur Passes##pb",    &m_genSettings.proceduralBlurPasses,  1, 0, 10);
        dirty |= ImGui::DragInt  ("Blur Radius##pb",    &m_genSettings.proceduralBlurRadius,  1, 1, 64);
        dirty |= ImGui::SliderFloat("Blur Strength##pb",&m_genSettings.proceduralBlurStrength,0.0f, 1.0f, "%.2f");

        if (ImGui::CollapsingHeader("Macro Landforms & Masks"))
        {
            dirty |= ImGui::DragFloat("Macro Scale",        &m_genSettings.macroScale,         0.0001f, 0.0001f, 0.05f, "%.4f");
            dirty |= ImGui::DragFloat("Macro Amplitude",    &m_genSettings.macroAmplitude,     0.01f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Broad Hill Scale",   &m_genSettings.broadHillScale,     0.0001f, 0.0001f, 0.05f, "%.4f");
            dirty |= ImGui::DragFloat("Broad Hill Str",     &m_genSettings.broadHillStrength,  0.01f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Valley Scale",       &m_genSettings.valleyScale,        0.0001f, 0.0001f, 0.05f, "%.4f");
            dirty |= ImGui::DragFloat("Valley Strength",    &m_genSettings.valleyStrength,     0.01f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Region Mask Scale",  &m_genSettings.regionMaskScale,    0.0001f, 0.0001f, 0.05f, "%.4f");
        }
        if (ImGui::CollapsingHeader("Rolling Hills"))
        {
            dirty |= ImGui::DragFloat("Hill Scale",       &m_genSettings.hillScale,        0.0001f, 0.0001f, 0.05f, "%.4f");
            dirty |= ImGui::DragFloat("Hill Amplitude",   &m_genSettings.hillAmplitude,    0.01f, 0.0f, 1.0f);
            dirty |= ImGui::DragInt  ("Hill Octaves",     &m_genSettings.hillOctaves,      1, 1, 8);
            dirty |= ImGui::DragFloat("Hill Persistence", &m_genSettings.hillPersistence,  0.01f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Hill Lacunarity",  &m_genSettings.hillLacunarity,   0.05f, 1.0f, 5.0f);
        }
        if (ImGui::CollapsingHeader("Mountain Layers"))
        {
            dirty |= ImGui::DragFloat("Mtn Mask Scale",  &m_genSettings.mountainRegionMaskScale, 0.0001f, 0.0001f, 0.05f, "%.4f");
            dirty |= ImGui::DragFloat("Mtn Scale",       &m_genSettings.mountainScale,           0.0001f, 0.0001f, 0.05f, "%.4f");
            dirty |= ImGui::DragFloat("Mtn Amplitude",   &m_genSettings.mountainAmplitude,       0.01f, 0.0f, 1.0f);
            dirty |= ImGui::DragInt  ("Mtn Octaves",     &m_genSettings.mountainOctaves,         1, 1, 8);
            dirty |= ImGui::DragFloat("Mtn Persistence", &m_genSettings.mountainPersistence,     0.01f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Mtn Lacunarity",  &m_genSettings.mountainLacunarity,      0.05f, 1.0f, 5.0f);
            dirty |= ImGui::DragFloat("Ridge Sharpness", &m_genSettings.ridgeSharpness,          0.1f, 0.5f, 6.0f);
            ImGui::Separator();
            dirty |= ImGui::DragFloat("Ridge Scale",       &m_genSettings.mountainRidgeScale,       0.0001f, 0.0001f, 0.05f, "%.4f");
            dirty |= ImGui::DragFloat("Ridge Strength",    &m_genSettings.mountainRidgeStrength,    0.01f, 0.0f, 1.0f);
            dirty |= ImGui::DragInt  ("Ridge Octaves",     &m_genSettings.mountainRidgeOctaves,     1, 1, 8);
            dirty |= ImGui::DragFloat("Ridge Persistence", &m_genSettings.mountainRidgePersistence, 0.01f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Ridge Lacunarity",  &m_genSettings.mountainRidgeLacunarity,  0.05f, 1.0f, 5.0f);
        }
        if (ImGui::CollapsingHeader("Plateau Layer"))
        {
            dirty |= ImGui::DragFloat("Plateau Region Scale", &m_genSettings.plateauRegionScale,      0.0001f, 0.0001f, 0.05f, "%.4f");
            dirty |= ImGui::DragFloat("Plateau Threshold",    &m_genSettings.plateauThreshold,        0.01f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Plateau Strength",     &m_genSettings.plateauStrength,         0.01f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Plateau Flattening",   &m_genSettings.plateauFlatteningAmount, 0.01f, 0.0f, 1.0f);
        }
        if (ImGui::CollapsingHeader("Detail Layer"))
        {
            dirty |= ImGui::DragFloat("Detail Scale",     &m_genSettings.detailScale,     0.001f, 0.001f, 0.1f, "%.3f");
            dirty |= ImGui::DragFloat("Detail Amplitude", &m_genSettings.detailAmplitude, 0.005f, 0.0f,  0.3f);
        }
        if (ImGui::CollapsingHeader("Volcano"))
        {
            dirty |= ImGui::Checkbox("Enabled##volc", &m_genSettings.volcanoEnabled);
            if (m_genSettings.volcanoEnabled)
            {
                dirty |= ImGui::DragFloat("Rim Radius##v",     &m_genSettings.volcanoRimRadius,         0.01f, 0.10f, 0.80f);
                dirty |= ImGui::DragFloat("Cone Height##v",    &m_genSettings.volcanoConeHeight,        0.01f, 0.00f, 1.50f);
                dirty |= ImGui::DragFloat("Caldera Depth##v",  &m_genSettings.volcanoCalderaDepth,      0.01f, 0.00f, 0.80f);
                dirty |= ImGui::DragFloat("Caldera Outer##v",  &m_genSettings.volcanoCalderaOuterRatio, 0.01f, 0.10f, 0.90f);
                dirty |= ImGui::DragFloat("Caldera Inner##v",  &m_genSettings.volcanoCalderaInnerRatio, 0.01f, 0.01f, 0.50f);
            }
        }
        if (ImGui::CollapsingHeader("Domain Warp"))
        {
            dirty |= ImGui::Checkbox("Enabled##dw", &m_genSettings.domainWarpEnabled);
            if (m_genSettings.domainWarpEnabled)
            {
                dirty |= ImGui::DragFloat("Warp Scale##w",    &m_genSettings.domainWarpScale,    0.0001f, 0.0001f, 0.02f, "%.4f");
                dirty |= ImGui::DragFloat("Warp Strength##w", &m_genSettings.domainWarpStrength, 1.0f, 0.0f, 120.0f);
            }
        }
        ImGui::Spacing();
    }

    // ── Erosion ──────────────────────────────────────────────────────────────
    if (SectionHeader("Erosion", false))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextFaint);
        ImGui::TextUnformatted("Hydraulic (particle droplets)");
        ImGui::PopStyleColor();
        dirty |= ImGui::Checkbox("Enable##eros", &m_genSettings.erosionEnabled);
        if (m_genSettings.erosionEnabled)
        {
            dirty |= ImGui::DragInt  ("Iterations##e",   &m_genSettings.erosionIterations, 1000, 1000, 500000);
            dirty |= ImGui::DragFloat("Capacity##e",     &m_genSettings.erosionCapacity,   0.1f, 0.5f, 20.0f);
            dirty |= ImGui::DragFloat("Inertia##e",      &m_genSettings.erosionInertia,    0.01f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Deposition##e",   &m_genSettings.erosionDeposition, 0.01f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Erosion Str##e",  &m_genSettings.erosionErosion,    0.01f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Evaporation##e",  &m_genSettings.erosionEvaporation,0.001f, 0.0f, 0.1f);
            dirty |= ImGui::DragFloat("Min Slope##e",    &m_genSettings.erosionMinSlope,   0.001f, 0.0f, 1.0f);
            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextFaint);
            ImGui::TextUnformatted("Post-erosion smoothing");
            ImGui::PopStyleColor();
            dirty |= ImGui::DragInt  ("Blur Passes##pe",    &m_genSettings.postErosionBlurPasses,     1, 0, 10);
            dirty |= ImGui::DragInt  ("Blur Radius##pe",    &m_genSettings.postErosionBlurRadius,     1, 1, 64);
            dirty |= ImGui::SliderFloat("Blur Str##pe",     &m_genSettings.postErosionBlurStrength,   0.0f, 1.0f, "%.2f");
            dirty |= ImGui::DragInt  ("Smooth Passes##pe2", &m_genSettings.postErosionSmoothingPasses,1, 0, 10);
        }
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextFaint);
        ImGui::TextUnformatted("Thermal (talus / rock-fall)");
        ImGui::PopStyleColor();
        dirty |= ImGui::Checkbox("Enable##th", &m_genSettings.thermalErosionEnabled);
        if (m_genSettings.thermalErosionEnabled)
        {
            dirty |= ImGui::DragInt  ("Iterations##th",     &m_genSettings.thermalErosionIterations, 1, 1, 50);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("3-8 is typical. More passes = smoother cliffs.");
            dirty |= ImGui::DragFloat("Repose Angle##th",   &m_genSettings.thermalErosionAngle,      0.5f, 5.0f, 75.0f, "%.1f deg");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("30-40 degrees is geologically typical.");
            dirty |= ImGui::SliderFloat("Strength##th",     &m_genSettings.thermalErosionStrength,   0.0f, 1.0f, "%.2f");
        }
        ImGui::Spacing();
    }

    // ── Thresholds ───────────────────────────────────────────────────────────
    if (SectionHeader("Thresholds", false))
    {
        if (ImGui::CollapsingHeader("Height Bands (0..1)"))
        {
            dirty |= ImGui::DragFloat("Deep Water",    &m_classSettings.deepWaterHeight,   0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Shallow Water", &m_classSettings.shallowWaterHeight,0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Sand",          &m_classSettings.sandHeight,        0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Grass Min S",   &m_classSettings.grassMinStart,     0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Grass Min E",   &m_classSettings.grassMinEnd,       0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Grass Max S",   &m_classSettings.grassMaxStart,     0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Grass Max E",   &m_classSettings.grassMaxEnd,       0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Forest Min S",  &m_classSettings.forestMinStart,    0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Forest Min E",  &m_classSettings.forestMinEnd,      0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Forest Max S",  &m_classSettings.forestMaxStart,    0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Forest Max E",  &m_classSettings.forestMaxEnd,      0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Mtn Start",     &m_classSettings.mountainStart,     0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Mtn Full",      &m_classSettings.mountainFull,      0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Snow Start",    &m_classSettings.snowStart,         0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Snow Full",     &m_classSettings.snowFull,          0.005f, 0.0f, 1.0f);
        }
        if (ImGui::CollapsingHeader("Slope Limits (0..1)"))
        {
            dirty |= ImGui::DragFloat("Sand Slope S",    &m_classSettings.sandSlopeStart,    0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Sand Slope E",    &m_classSettings.sandSlopeEnd,      0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Grass Slope S",   &m_classSettings.grassSlopeStart,   0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Grass Slope E",   &m_classSettings.grassSlopeEnd,     0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Forest Slope S",  &m_classSettings.forestSlopeStart,  0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Forest Slope E",  &m_classSettings.forestSlopeEnd,    0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Rock Slope S",    &m_classSettings.rockSlopeStart,    0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Rock Slope E",    &m_classSettings.rockSlopeEnd,      0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Snow Slope S",    &m_classSettings.snowSlopeStart,    0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Snow Slope E",    &m_classSettings.snowSlopeEnd,      0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Excl Slope S",    &m_classSettings.excludeSlopeStart, 0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Excl Slope E",    &m_classSettings.excludeSlopeEnd,   0.005f, 0.0f, 1.0f);
        }
        if (ImGui::CollapsingHeader("Vegetation Masks"))
        {
            dirty |= ImGui::DragFloat("Tree Min S",       &m_classSettings.treeMinStart,      0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Tree Min E",       &m_classSettings.treeMinEnd,        0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Tree Max S",       &m_classSettings.treeMaxStart,      0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Tree Max E",       &m_classSettings.treeMaxEnd,        0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Tree Slope S",     &m_classSettings.treeSlopeStart,    0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Tree Slope E",     &m_classSettings.treeSlopeEnd,      0.005f, 0.0f, 1.0f);
            ImGui::Separator();
            dirty |= ImGui::DragFloat("Rock Mask H S",    &m_classSettings.rockMaskHeightStart,0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Rock Mask H E",    &m_classSettings.rockMaskHeightEnd,  0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Rock Mask Slope S",&m_classSettings.rockMaskSlopeStart, 0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Rock Mask Slope E",&m_classSettings.rockMaskSlopeEnd,   0.005f, 0.0f, 1.0f);
        }
        ImGui::Spacing();
    }

    // ── Vegetation ───────────────────────────────────────────────────────────
    if (SectionHeader("Vegetation"))
    {
        ImGui::Checkbox("Auto-scale##veg", &m_vegetation.m_autoScale);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "Scales each prop by world size, elevation, slope, and erosion amount\n"
                "so vegetation looks proportional regardless of heightScale or worldWidth.");
        m_vegetation.OnImGui();
        ImGui::Spacing();
    }

    if (dirty)
        m_settingsDirty = true;
}

void TerrainScene::OnTerrainTabFooter()
{
    // [Volcano] [Auto] [───────── Regenerate ─────────]
    // * unsaved changes  (amber, only when dirty)

    // Volcano preset button — warm tinted
    ImGui::PushStyleColor(ImGuiCol_Button,        {0.22f, 0.17f, 0.06f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.32f, 0.25f, 0.08f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  {0.16f, 0.12f, 0.04f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_Text, Pal::Amber);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    if (ImGui::Button("Volcano##footer", ImVec2(64, 0)))
    {
        m_genSettings.volcanoEnabled           = true;
        m_genSettings.volcanoRimRadius         = 0.38f;
        m_genSettings.volcanoConeHeight        = 0.72f;
        m_genSettings.volcanoCalderaDepth      = 0.28f;
        m_genSettings.volcanoCalderaOuterRatio = 0.60f;
        m_genSettings.volcanoCalderaInnerRatio = 0.15f;
        m_genSettings.macroAmplitude           = 0.15f;
        m_genSettings.hillAmplitude            = 0.10f;
        m_genSettings.detailAmplitude          = 0.03f;
        m_genSettings.valleyStrength           = 0.05f;
        m_genSettings.plateauStrength          = 0.40f;
        m_genSettings.plateauThreshold         = 0.08f;
        m_genSettings.domainWarpEnabled        = true;
        m_genSettings.domainWarpScale          = 0.003f;
        m_genSettings.domainWarpStrength       = 25.0f;
        m_classSettings.snowStart              = 0.90f;
        m_classSettings.snowFull               = 0.97f;
        m_classSettings.forestMaxStart         = 0.45f;
        m_classSettings.forestMaxEnd           = 0.58f;
        m_needsRegenerate = true;
        m_settingsDirty   = false;
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);

    ImGui::SameLine();

    // Auto-regen toggle — lit up when active
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    if (m_autoRegenerate)
    {
        ImGui::PushStyleColor(ImGuiCol_Button,        {0.00f, 0.28f, 0.60f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.00f, 0.34f, 0.72f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextHi);
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Button,        Pal::Bg3);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.22f, 0.22f, 0.25f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_Text, Pal::TextDim);
    }
    if (ImGui::Button("Auto##ar", ImVec2(42, 0)))
        m_autoRegenerate = !m_autoRegenerate;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Regenerate immediately whenever a setting changes.");
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    ImGui::SameLine();

    // Main Regenerate button — accent blue, fills remaining width
    ImGui::PushStyleColor(ImGuiCol_Button,        {0.00f, 0.42f, 0.90f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.10f, 0.52f, 1.00f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  {0.00f, 0.34f, 0.76f, 1.f});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    const bool pressed = ImGui::Button("Regenerate##regen", ImVec2(-1, 0));
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    if (m_settingsDirty)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, Pal::Amber);
        ImGui::TextUnformatted("  * unsaved changes");
        ImGui::PopStyleColor();
    }

    if (pressed || (m_autoRegenerate && m_settingsDirty))
    {
        m_needsRegenerate = true;
        m_settingsDirty   = false;
    }
}
