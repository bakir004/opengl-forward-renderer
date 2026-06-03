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
    ImGui::SetNextWindowSize({420, 520}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Terrain Controls"))
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

    // ── Generation parameters ────────────────────────────────────────────────
    ImGui::SeparatorText("Generation");

    bool dirty = false;
    dirty |= ImGui::DragInt("Seed",   reinterpret_cast<int*>(&m_genSettings.seed));
    dirty |= ImGui::DragInt("Grid W", reinterpret_cast<int*>(&m_genSettings.gridWidth),  1.0f, 32, 512);
    dirty |= ImGui::DragInt("Grid H", reinterpret_cast<int*>(&m_genSettings.gridHeight), 1.0f, 32, 512);
    dirty |= ImGui::DragFloat("World Width",  &m_genSettings.worldWidth,  1.0f, 64.0f, 2048.0f);
    dirty |= ImGui::DragFloat("World Height (Z)", &m_genSettings.worldHeight, 1.0f, 64.0f, 2048.0f);
    dirty |= ImGui::DragFloat("Height Scale", &m_genSettings.heightScale, 0.5f, 10.0f, 500.0f);

    ImGui::Spacing();
    ImGui::Text("Rolling Hills");
    dirty |= ImGui::DragFloat("Hill Scale",    &m_genSettings.hillScale,     0.0001f, 0.0001f, 0.05f, "%.4f");
    dirty |= ImGui::DragFloat("Hill Amplitude",&m_genSettings.hillAmplitude, 0.01f,  0.0f, 1.0f);
    dirty |= ImGui::DragInt  ("Hill Octaves",  &m_genSettings.hillOctaves,   1.0f,   1, 8);

    ImGui::Spacing();
    ImGui::Text("Mountains");
    dirty |= ImGui::DragFloat("Mountain Amplitude", &m_genSettings.mountainAmplitude, 0.01f, 0.0f, 1.0f);
    dirty |= ImGui::DragFloat("Ridge Sharpness",    &m_genSettings.ridgeSharpness,    0.1f,  0.5f, 6.0f);

    ImGui::Spacing();
    ImGui::Text("Detail");
    dirty |= ImGui::DragFloat("Detail Scale",    &m_genSettings.detailScale,     0.001f, 0.001f, 0.1f, "%.3f");
    dirty |= ImGui::DragFloat("Detail Amplitude",&m_genSettings.detailAmplitude, 0.005f, 0.0f,  0.3f);

    // ── Classification thresholds ─────────────────────────────────────────────
    ImGui::SeparatorText("Material Thresholds");
    dirty |= ImGui::DragFloat("Grass Max H",  &m_classSettings.grassMaxStart,  0.01f, 0.0f, 1.0f);
    dirty |= ImGui::DragFloat("Forest Max H", &m_classSettings.forestMaxStart, 0.01f, 0.0f, 1.0f);
    dirty |= ImGui::DragFloat("Snow Start H", &m_classSettings.snowStart,      0.01f, 0.0f, 1.0f);
    dirty |= ImGui::DragFloat("Rock Slope",   &m_classSettings.rockSlopeStart, 0.01f, 0.0f, 1.0f);

    // ── Volcano ───────────────────────────────────────────────────────────────
    ImGui::SeparatorText("Volcano");
    dirty |= ImGui::Checkbox("Volcano Enabled", &m_genSettings.volcanoEnabled);
    if (m_genSettings.volcanoEnabled)
    {
        dirty |= ImGui::DragFloat("Rim Radius##v",    &m_genSettings.volcanoRimRadius,         0.01f, 0.10f, 0.80f);
        dirty |= ImGui::DragFloat("Cone Height##v",   &m_genSettings.volcanoConeHeight,        0.01f, 0.00f, 1.50f);
        dirty |= ImGui::DragFloat("Caldera Depth##v", &m_genSettings.volcanoCalderaDepth,      0.01f, 0.00f, 0.80f);
        dirty |= ImGui::DragFloat("Caldera Outer##v", &m_genSettings.volcanoCalderaOuterRatio, 0.01f, 0.10f, 0.90f);
        dirty |= ImGui::DragFloat("Caldera Inner##v", &m_genSettings.volcanoCalderaInnerRatio, 0.01f, 0.01f, 0.50f);
    }

    // ── Domain Warp ───────────────────────────────────────────────────────────
    ImGui::SeparatorText("Domain Warp");
    dirty |= ImGui::Checkbox("Warp Enabled", &m_genSettings.domainWarpEnabled);
    if (m_genSettings.domainWarpEnabled)
    {
        dirty |= ImGui::DragFloat("Warp Scale##w",    &m_genSettings.domainWarpScale,    0.0001f, 0.0001f, 0.02f, "%.4f");
        dirty |= ImGui::DragFloat("Warp Strength##w", &m_genSettings.domainWarpStrength, 1.0f,    0.0f,    120.0f);
    }

    // ── Erosion ───────────────────────────────────────────────────────────────
    ImGui::SeparatorText("Erosion (Stretch)");
    dirty |= ImGui::Checkbox("Enable Erosion", &m_genSettings.erosionEnabled);
    if (m_genSettings.erosionEnabled)
    {
        dirty |= ImGui::DragInt  ("Erosion Iterations", &m_genSettings.erosionIterations, 1000, 1000, 500000);
        dirty |= ImGui::DragFloat("Erosion Capacity",   &m_genSettings.erosionCapacity,   0.1f, 0.5f, 20.0f);
        dirty |= ImGui::DragFloat("Erosion Inertia",    &m_genSettings.erosionInertia,    0.01f, 0.0f, 1.0f);
        dirty |= ImGui::DragFloat("Erosion Deposition", &m_genSettings.erosionDeposition, 0.01f, 0.0f, 1.0f);
        dirty |= ImGui::DragFloat("Evaporation",        &m_genSettings.erosionEvaporation,0.001f, 0.0f, 0.1f);
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
