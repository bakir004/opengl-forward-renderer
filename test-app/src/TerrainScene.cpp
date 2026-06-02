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

// ─── Zone PBR properties ─────────────────────────────────────────────────────
// Order matches TerrainMaterialZone enum: DeepWater, ShallowWater, Sand, Grass, Forest, Rock, Snow

static const glm::vec3 kZoneColor[7] = {
    {0.04f, 0.10f, 0.35f},  // DeepWater
    {0.10f, 0.45f, 0.60f},  // ShallowWater
    {0.82f, 0.72f, 0.50f},  // Sand
    {0.28f, 0.55f, 0.12f},  // Grass
    {0.10f, 0.30f, 0.08f},  // Forest
    {0.46f, 0.42f, 0.38f},  // Rock
    {0.93f, 0.95f, 0.97f},  // Snow
};

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
    SetAmbientLight({0.55f, 0.62f, 0.72f}, 0.40f);

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

    // Apply the per-frame zone uniforms via the shader directly.
    // We do this once here; debug-view toggle re-applies in OnUpdate via
    // ApplyMaterialUniforms(), called each frame.

    // Rebuild the RenderItem (or add it on first call).
    RenderItem item;
    item.mesh     = m_terrainBuffer.get();
    item.material = m_terrainInstance.get();
    item.flags.castShadow    = true;
    item.flags.receiveShadow = true;
    item.transform.SetTranslation({0.0f, 0.0f, 0.0f});

    if (!m_terrainAdded)
    {
        // First time — add the object and record its index.
        m_terrainObjectIndex = AddObject(item);
        m_terrainAdded = true;
    }
    else
    {
        // Subsequent regeneration — patch the existing item.
        GetObject(m_terrainObjectIndex).mesh = m_terrainBuffer.get();
    }
}

void TerrainScene::ApplyMaterialUniforms() const
{
    if (!m_terrainShader || !m_terrainShader->IsValid())
        return;

    m_terrainShader->Bind();

    // Zone colours and PBR scalars
    for (int i = 0; i < 7; ++i)
    {
        m_terrainShader->SetUniform("u_ZoneColor[" + std::to_string(i) + "]",   kZoneColor[i]);
        m_terrainShader->SetUniform("u_ZoneRoughness[" + std::to_string(i) + "]", kZoneRoughness[i]);
        m_terrainShader->SetUniform("u_ZoneMetallic[" + std::to_string(i) + "]",  kZoneMetallic[i]);
    }

    m_terrainShader->SetUniform("u_DebugView", static_cast<int>(m_debugView));

    ShaderProgram::Unbind();
}

void TerrainScene::OnUpdate(float deltaTime, IInputProvider& input)
{
    UpdateStandardCameraAndPlayer(deltaTime, input, m_playerPos, m_moveDirXZ);

    // Apply uniforms that may have changed via ImGui each frame.
    ApplyMaterialUniforms();

    if (m_needsRegenerate)
    {
        m_needsRegenerate = false;
        Regenerate();
    }
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

    // ── Generation parameters ────────────────────────────────────────────────
    ImGui::SeparatorText("Generation");

    bool changed = false;
    changed |= ImGui::DragInt("Seed",   reinterpret_cast<int*>(&m_genSettings.seed));
    changed |= ImGui::DragInt("Grid W", reinterpret_cast<int*>(&m_genSettings.gridWidth),  1.0f, 32, 512);
    changed |= ImGui::DragInt("Grid H", reinterpret_cast<int*>(&m_genSettings.gridHeight), 1.0f, 32, 512);
    changed |= ImGui::DragFloat("World Width",  &m_genSettings.worldWidth,  1.0f, 64.0f, 2048.0f);
    changed |= ImGui::DragFloat("World Height (Z)", &m_genSettings.worldHeight, 1.0f, 64.0f, 2048.0f);
    changed |= ImGui::DragFloat("Height Scale", &m_genSettings.heightScale, 0.5f, 10.0f, 500.0f);

    ImGui::Spacing();
    ImGui::Text("Rolling Hills");
    changed |= ImGui::DragFloat("Hill Scale",    &m_genSettings.hillScale,     0.0001f, 0.0001f, 0.05f, "%.4f");
    changed |= ImGui::DragFloat("Hill Amplitude",&m_genSettings.hillAmplitude, 0.01f,  0.0f, 1.0f);
    changed |= ImGui::DragInt  ("Hill Octaves",  &m_genSettings.hillOctaves,   1.0f,   1, 8);

    ImGui::Spacing();
    ImGui::Text("Mountains");
    changed |= ImGui::DragFloat("Mountain Amplitude", &m_genSettings.mountainAmplitude, 0.01f, 0.0f, 1.0f);
    changed |= ImGui::DragFloat("Ridge Sharpness",    &m_genSettings.ridgeSharpness,    0.1f,  0.5f, 6.0f);

    ImGui::Spacing();
    ImGui::Text("Detail");
    changed |= ImGui::DragFloat("Detail Scale",    &m_genSettings.detailScale,     0.001f, 0.001f, 0.1f, "%.3f");
    changed |= ImGui::DragFloat("Detail Amplitude",&m_genSettings.detailAmplitude, 0.005f, 0.0f,  0.3f);

    // ── Classification thresholds ─────────────────────────────────────────────
    ImGui::SeparatorText("Material Thresholds");
    changed |= ImGui::DragFloat("Grass Max H",  &m_classSettings.grassMaxStart,  0.01f, 0.0f, 1.0f);
    changed |= ImGui::DragFloat("Forest Max H", &m_classSettings.forestMaxStart, 0.01f, 0.0f, 1.0f);
    changed |= ImGui::DragFloat("Snow Start H", &m_classSettings.snowStart,      0.01f, 0.0f, 1.0f);
    changed |= ImGui::DragFloat("Rock Slope",   &m_classSettings.rockSlopeStart, 0.01f, 0.0f, 1.0f);

    // ── Erosion ───────────────────────────────────────────────────────────────
    ImGui::SeparatorText("Erosion (Stretch)");
    changed |= ImGui::Checkbox("Enable Erosion", &m_genSettings.erosionEnabled);
    if (m_genSettings.erosionEnabled)
    {
        changed |= ImGui::DragInt  ("Erosion Iterations", &m_genSettings.erosionIterations, 1000, 1000, 500000);
        changed |= ImGui::DragFloat("Erosion Capacity",   &m_genSettings.erosionCapacity,   0.1f, 0.5f, 20.0f);
        changed |= ImGui::DragFloat("Erosion Inertia",    &m_genSettings.erosionInertia,    0.01f, 0.0f, 1.0f);
        changed |= ImGui::DragFloat("Erosion Deposition", &m_genSettings.erosionDeposition, 0.01f, 0.0f, 1.0f);
        changed |= ImGui::DragFloat("Evaporation",        &m_genSettings.erosionEvaporation,0.001f, 0.0f, 0.1f);
    }

    // ── Regenerate button ─────────────────────────────────────────────────────
    ImGui::Spacing();
    if (ImGui::Button("Regenerate") || (changed && false))
        m_needsRegenerate = true;

    ImGui::SameLine();
    ImGui::TextDisabled("(or press Regenerate after changing params)");

    ImGui::End();
}
