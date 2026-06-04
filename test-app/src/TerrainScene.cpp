#include "TerrainScene.h"

#include "assets/AssetImporter.h"
#include "core/Camera.h"
#include "core/IInputProvider.h"
#include "core/Material.h"
#include "core/ShaderProgram.h"
#include "scene/LightBuilder.h"
#include "scene/RenderItem.h"

#include <imgui.h>
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
    item.transform.SetTranslation({0.0f, 0.0f, 0.0f});

    if (!m_terrainAdded)
    {
        m_terrainObjectIndex = AddObject(item);
        m_terrainAdded = true;
    }
    else
    {
        RenderItem& existing = GetObject(m_terrainObjectIndex);
        existing.mesh = m_terrainBuffer.get();
    }
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

    // Per-frame frustum cull + SSBO upload for all vegetation groups.
    const Camera& cam = GetCamera();
    m_vegetation.CullAndUpload(cam.GetViewProjection(), cam.GetPosition());
}

void TerrainScene::OnPostRender()
{
    m_vegetation.DrawAll();
}

void TerrainScene::OnImGuiRender()
{
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(displaySize.x - 15.0f, 15.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(420.0f, 500.0f), ImGuiCond_Always);
    if (!ImGui::Begin("Terrain Controls", nullptr, ImGuiWindowFlags_NoMove))
    {
        ImGui::End();
        return;
    }

    // ── Stats ─────────────────────────────────────────────────────────────────
    ImGui::SeparatorText("Stats");
    ImGui::Text("Seed:        %s",     m_stats.lastSeedStr.c_str());
    ImGui::Text("Vertices:    %u",     m_stats.vertexCount);
    ImGui::Text("Indices:     %u",     m_stats.indexCount);
    ImGui::Text("Triangles:   %u",     m_stats.triangleCount);
    ImGui::Text("Gen time:    %.2f ms", m_stats.generationTimeMs);
    ImGui::Text("Build time:  %.2f ms", m_stats.meshBuildTimeMs);
    ImGui::Text("Upload time: %.2f ms", m_stats.meshUploadTimeMs);

    // ── Debug view ───────────────────────────────────────────────────────────
    ImGui::SeparatorText("Debug View");
    static const char* kViewNames[] = {
        "Off", "Heightmap", "Slope", "Normals",
        "Material Zone", "Mountain Mask", "Grass Mask", "Tree Mask", "Rock Mask"
    };
    int viewIdx = static_cast<int>(m_debugView);
    if (ImGui::Combo("View", &viewIdx, kViewNames, IM_ARRAYSIZE(kViewNames)))
        m_debugView = static_cast<TerrainDebugView>(viewIdx);

    // ── Atmosphere ───────────────────────────────────────────────────────────
    ImGui::SeparatorText("Atmosphere");
    ImGui::DragFloat("Fog Density##atm", &m_fogDensity, 0.0001f, 0.0f, 0.02f, "%.4f");
    ImGui::ColorEdit3("Fog Color##atm",  &m_fogColor.x);

    // ── Heightmap input ──────────────────────────────────────────────────────
    ImGui::SeparatorText("Heightmap Input");

    bool dirty = false;
    dirty |= ImGui::Checkbox("Use PNG Heightmap##hm", &m_genSettings.useHeightmap);
    if (m_genSettings.useHeightmap)
    {
        static char s_pathBuf[512] = {};
        if (s_pathBuf[0] == '\0')
            strncpy(s_pathBuf, m_genSettings.heightmapPath.c_str(), sizeof(s_pathBuf) - 1);
        if (ImGui::InputText("Path##hm", s_pathBuf, sizeof(s_pathBuf)))
        {
            m_genSettings.heightmapPath = s_pathBuf;
            dirty = true;
        }
        dirty |= ImGui::DragFloat("Gamma##hm",  &m_genSettings.heightmapGamma, 0.01f, 0.1f, 4.0f, "%.2f");
        dirty |= ImGui::Checkbox ("Flip Y##hm", &m_genSettings.heightmapFlipY);
    }

    // ── Generation parameters ────────────────────────────────────────────────
    ImGui::SeparatorText("Generation");
    dirty |= ImGui::DragInt("Seed",   reinterpret_cast<int*>(&m_genSettings.seed));
    dirty |= ImGui::DragInt("Grid W", reinterpret_cast<int*>(&m_genSettings.gridWidth),  1.0f, 32, 512);
    dirty |= ImGui::DragInt("Grid H", reinterpret_cast<int*>(&m_genSettings.gridHeight), 1.0f, 32, 512);
    dirty |= ImGui::DragFloat("World Width",  &m_genSettings.worldWidth,  1.0f, 64.0f, 2048.0f);
    dirty |= ImGui::DragFloat("World Height (Z)", &m_genSettings.worldHeight, 1.0f, 64.0f, 2048.0f);
    dirty |= ImGui::DragFloat("Height Scale", &m_genSettings.heightScale, 0.5f, 10.0f, 500.0f);

    if (ImGui::CollapsingHeader("Macro Landforms & Masks"))
    {
        dirty |= ImGui::DragFloat("Macro Scale", &m_genSettings.macroScale, 0.0001f, 0.0001f, 0.05f, "%.4f");
        dirty |= ImGui::DragFloat("Macro Amplitude", &m_genSettings.macroAmplitude, 0.01f, 0.0f, 1.0f);
        dirty |= ImGui::DragFloat("Broad Hill Scale", &m_genSettings.broadHillScale, 0.0001f, 0.0001f, 0.05f, "%.4f");
        dirty |= ImGui::DragFloat("Broad Hill Strength", &m_genSettings.broadHillStrength, 0.01f, 0.0f, 1.0f);
        dirty |= ImGui::DragFloat("Valley Scale", &m_genSettings.valleyScale, 0.0001f, 0.0001f, 0.05f, "%.4f");
        dirty |= ImGui::DragFloat("Valley Strength", &m_genSettings.valleyStrength, 0.01f, 0.0f, 1.0f);
        dirty |= ImGui::DragFloat("Region Mask Scale", &m_genSettings.regionMaskScale, 0.0001f, 0.0001f, 0.05f, "%.4f");
    }

    if (ImGui::CollapsingHeader("Rolling Hills"))
    {
        dirty |= ImGui::DragFloat("Hill Scale",    &m_genSettings.hillScale,     0.0001f, 0.0001f, 0.05f, "%.4f");
        dirty |= ImGui::DragFloat("Hill Amplitude",&m_genSettings.hillAmplitude, 0.01f,  0.0f, 1.0f);
        dirty |= ImGui::DragInt  ("Hill Octaves",  &m_genSettings.hillOctaves,   1.0f,   1, 8);
        dirty |= ImGui::DragFloat("Hill Persistence", &m_genSettings.hillPersistence, 0.01f, 0.0f, 1.0f);
        dirty |= ImGui::DragFloat("Hill Lacunarity", &m_genSettings.hillLacunarity, 0.05f, 1.0f, 5.0f);
    }

    if (ImGui::CollapsingHeader("Mountain Layers"))
    {
        dirty |= ImGui::DragFloat("Mtn Region Mask Scale", &m_genSettings.mountainRegionMaskScale, 0.0001f, 0.0001f, 0.05f, "%.4f");
        dirty |= ImGui::DragFloat("Mountain Scale", &m_genSettings.mountainScale, 0.0001f, 0.0001f, 0.05f, "%.4f");
        dirty |= ImGui::DragFloat("Mountain Amplitude", &m_genSettings.mountainAmplitude, 0.01f, 0.0f, 1.0f);
        dirty |= ImGui::DragInt("Mountain Octaves", &m_genSettings.mountainOctaves, 1.0f, 1, 8);
        dirty |= ImGui::DragFloat("Mountain Persistence", &m_genSettings.mountainPersistence, 0.01f, 0.0f, 1.0f);
        dirty |= ImGui::DragFloat("Mountain Lacunarity", &m_genSettings.mountainLacunarity, 0.05f, 1.0f, 5.0f);
        dirty |= ImGui::DragFloat("Ridge Sharpness",    &m_genSettings.ridgeSharpness,    0.1f,  0.5f, 6.0f);
        ImGui::Separator();
        dirty |= ImGui::DragFloat("Mtn Ridge Scale", &m_genSettings.mountainRidgeScale, 0.0001f, 0.0001f, 0.05f, "%.4f");
        dirty |= ImGui::DragFloat("Mtn Ridge Strength", &m_genSettings.mountainRidgeStrength, 0.01f, 0.0f, 1.0f);
        dirty |= ImGui::DragInt("Mtn Ridge Octaves", &m_genSettings.mountainRidgeOctaves, 1.0f, 1, 8);
        dirty |= ImGui::DragFloat("Mtn Ridge Persistence", &m_genSettings.mountainRidgePersistence, 0.01f, 0.0f, 1.0f);
        dirty |= ImGui::DragFloat("Mtn Ridge Lacunarity", &m_genSettings.mountainRidgeLacunarity, 0.05f, 1.0f, 5.0f);
    }

    if (ImGui::CollapsingHeader("Plateau Layer"))
    {
        dirty |= ImGui::DragFloat("Plateau Region Scale", &m_genSettings.plateauRegionScale, 0.0001f, 0.0001f, 0.05f, "%.4f");
        dirty |= ImGui::DragFloat("Plateau Threshold", &m_genSettings.plateauThreshold, 0.01f, 0.0f, 1.0f);
        dirty |= ImGui::DragFloat("Plateau Strength", &m_genSettings.plateauStrength, 0.01f, 0.0f, 1.0f);
        dirty |= ImGui::DragFloat("Plateau Flattening", &m_genSettings.plateauFlatteningAmount, 0.01f, 0.0f, 1.0f);
    }

    if (ImGui::CollapsingHeader("Detail Layer"))
    {
        dirty |= ImGui::DragFloat("Detail Scale",    &m_genSettings.detailScale,     0.001f, 0.001f, 0.1f, "%.3f");
        dirty |= ImGui::DragFloat("Detail Amplitude",&m_genSettings.detailAmplitude, 0.005f, 0.0f,  0.3f);
    }

    if (ImGui::CollapsingHeader("Volcano Layer"))
    {
        dirty |= ImGui::Checkbox("Volcano Enabled##volc", &m_genSettings.volcanoEnabled);
        if (m_genSettings.volcanoEnabled)
        {
            dirty |= ImGui::DragFloat("Rim Radius##v",    &m_genSettings.volcanoRimRadius,         0.01f, 0.10f, 0.80f);
            dirty |= ImGui::DragFloat("Cone Height##v",   &m_genSettings.volcanoConeHeight,        0.01f, 0.00f, 1.50f);
            dirty |= ImGui::DragFloat("Caldera Depth##v", &m_genSettings.volcanoCalderaDepth,      0.01f, 0.00f, 0.80f);
            dirty |= ImGui::DragFloat("Caldera Outer##v", &m_genSettings.volcanoCalderaOuterRatio, 0.01f, 0.10f, 0.90f);
            dirty |= ImGui::DragFloat("Caldera Inner##v", &m_genSettings.volcanoCalderaInnerRatio, 0.01f, 0.01f, 0.50f);
        }
    }

    if (ImGui::CollapsingHeader("Domain Warp Layer"))
    {
        dirty |= ImGui::Checkbox("Warp Enabled##dw", &m_genSettings.domainWarpEnabled);
        if (m_genSettings.domainWarpEnabled)
        {
            dirty |= ImGui::DragFloat("Warp Scale##w",    &m_genSettings.domainWarpScale,    0.0001f, 0.0001f, 0.02f, "%.4f");
            dirty |= ImGui::DragFloat("Warp Strength##w", &m_genSettings.domainWarpStrength, 1.0f,    0.0f,    120.0f);
        }
    }

    if (ImGui::CollapsingHeader("Erosion Layer (Stretch)"))
    {
        dirty |= ImGui::Checkbox("Enable Erosion##eros", &m_genSettings.erosionEnabled);
        if (m_genSettings.erosionEnabled)
        {
            dirty |= ImGui::DragInt  ("Erosion Iterations", &m_genSettings.erosionIterations, 1000, 1000, 500000);
            dirty |= ImGui::DragFloat("Erosion Capacity",   &m_genSettings.erosionCapacity,   0.1f, 0.5f, 20.0f);
            dirty |= ImGui::DragFloat("Erosion Inertia",    &m_genSettings.erosionInertia,    0.01f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Erosion Deposition", &m_genSettings.erosionDeposition, 0.01f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Evaporation",        &m_genSettings.erosionEvaporation,0.001f, 0.0f, 0.1f);
        }
    }

    // ── Classification thresholds ─────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Material & Mask Thresholds"))
    {
        if (ImGui::TreeNode("Height Bands (0..1)"))
        {
            dirty |= ImGui::DragFloat("Deep Water Height", &m_classSettings.deepWaterHeight, 0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Shallow Water Height", &m_classSettings.shallowWaterHeight, 0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Sand Height", &m_classSettings.sandHeight, 0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Grass Min Start", &m_classSettings.grassMinStart, 0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Grass Min End", &m_classSettings.grassMinEnd, 0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Grass Max Start", &m_classSettings.grassMaxStart, 0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Grass Max End", &m_classSettings.grassMaxEnd, 0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Forest Min Start", &m_classSettings.forestMinStart, 0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Forest Min End", &m_classSettings.forestMinEnd, 0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Forest Max Start", &m_classSettings.forestMaxStart, 0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Forest Max End", &m_classSettings.forestMaxEnd, 0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Mountain Start", &m_classSettings.mountainStart, 0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Mountain Full", &m_classSettings.mountainFull, 0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Snow Start", &m_classSettings.snowStart, 0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Snow Full", &m_classSettings.snowFull, 0.005f, 0.0f, 1.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Slope Limits (0..1)"))
        {
            dirty |= ImGui::DragFloat("Sand Slope Start", &m_classSettings.sandSlopeStart, 0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Sand Slope End", &m_classSettings.sandSlopeEnd, 0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Grass Slope Start", &m_classSettings.grassSlopeStart, 0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Grass Slope End", &m_classSettings.grassSlopeEnd, 0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Forest Slope Start", &m_classSettings.forestSlopeStart, 0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Forest Slope End", &m_classSettings.forestSlopeEnd, 0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Rock Slope Start", &m_classSettings.rockSlopeStart, 0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Rock Slope End", &m_classSettings.rockSlopeEnd, 0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Snow Slope Start", &m_classSettings.snowSlopeStart, 0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Snow Slope End", &m_classSettings.snowSlopeEnd, 0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Exclude Slope Start", &m_classSettings.excludeSlopeStart, 0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Exclude Slope End", &m_classSettings.excludeSlopeEnd, 0.005f, 0.0f, 1.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Vegetation Constraints"))
        {
            dirty |= ImGui::DragFloat("Tree Min Start", &m_classSettings.treeMinStart, 0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Tree Min End", &m_classSettings.treeMinEnd, 0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Tree Max Start", &m_classSettings.treeMaxStart, 0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Tree Max End", &m_classSettings.treeMaxEnd, 0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Tree Slope Start", &m_classSettings.treeSlopeStart, 0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Tree Slope End", &m_classSettings.treeSlopeEnd, 0.005f, 0.0f, 1.0f);
            ImGui::Separator();
            dirty |= ImGui::DragFloat("Rock Mask Height Start", &m_classSettings.rockMaskHeightStart, 0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Rock Mask Height End", &m_classSettings.rockMaskHeightEnd, 0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Rock Mask Slope Start", &m_classSettings.rockMaskSlopeStart, 0.005f, 0.0f, 1.0f);
            dirty |= ImGui::DragFloat("Rock Mask Slope End", &m_classSettings.rockMaskSlopeEnd, 0.005f, 0.0f, 1.0f);
            ImGui::TreePop();
        }
    }

    if (dirty)
        m_settingsDirty = true;

    // ── Regenerate ─────────────────────────────────────────────────────────────
    ImGui::SeparatorText("Apply");

    if (ImGui::Button("Volcano Preset"))
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

    ImGui::Checkbox("Auto-regenerate", &m_autoRegenerate);

    const bool pressed = ImGui::Button("Regenerate");
    if (pressed || (m_autoRegenerate && m_settingsDirty))
    {
        m_needsRegenerate = true;
        m_settingsDirty   = false;
    }

    if (m_settingsDirty)
    {
        ImGui::SameLine();
        ImGui::TextColored({1.0f, 0.8f, 0.2f, 1.0f}, "* unsaved changes");
    }

    // ── Vegetation ────────────────────────────────────────────────────────────
    m_vegetation.OnImGui();

    ImGui::End();
}
