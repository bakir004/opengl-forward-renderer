#pragma once

#include "scene/Scene.h"
#include "terrain/TerrainData.h"
#include "terrain/TerrainGenerator.h"
#include "terrain/TerrainMeshBuilder.h"
#include "core/Material.h"
#include "terrain/TerrainVegetation.h"

#include <memory>
#include <vector>

class MeshBuffer;
class ShaderProgram;

/// Debug view modes exposed in ImGui (must match terrain.frag constants).
enum class TerrainDebugView : int
{
    Off = 0,
    Heightmap = 1,
    Slope = 2,
    Normals = 3,
    MaterialZone = 4,
    MountainMask = 5,
    GrassMask = 6,
    TreeMask = 7,
    RockMask = 8,
};

/// Sprint 10 — T6: terrain renderer integration.
///
/// Submits a generated terrain mesh through the standard Scene/RenderItem
/// submission API.  No special renderer code path is required.
class TerrainScene : public Scene
{
public:
    bool Setup();
    void OnUpdate(float deltaTime, IInputProvider &input) override;
    void OnPostRender() override;
    void OnImGuiRender() override;

private:
    void Regenerate();
    void UploadToGpu();
    void ApplyMaterialUniforms() const;

    // ── Generation data ────────────────────────────────────────────────────
    TerrainGenerationSettings m_genSettings;
    TerrainClassificationSettings m_classSettings;
    TerrainHeightfield m_heightfield;
    TerrainMeshData m_meshData;
    TerrainStats m_stats;

    // ── GPU objects ────────────────────────────────────────────────────────
    std::unique_ptr<MeshBuffer> m_terrainBuffer;
    std::shared_ptr<ShaderProgram> m_terrainShader;
    std::shared_ptr<Material> m_terrainMaterial;
    std::unique_ptr<MaterialInstance> m_terrainInstance;

    // Scene render-item index
    size_t m_terrainObjectIndex = 0;

    // ── Camera ─────────────────────────────────────────────────────────────
    glm::vec3 m_playerPos{0.0f, 0.0f, 0.0f};
    glm::vec3 m_moveDirXZ{0.0f, 0.0f, 0.0f};

    // ── Vegetation ─────────────────────────────────────────────────────────
    TerrainVegetation m_vegetation;

    // ── Atmosphere ─────────────────────────────────────────────────────────
    float     m_fogDensity = 0.0015f;
    glm::vec3 m_fogColor   {0.58f, 0.65f, 0.78f};

    // ── Debug ──────────────────────────────────────────────────────────────
    TerrainDebugView m_debugView = TerrainDebugView::Off;
    bool m_needsRegenerate = false;
    bool m_terrainAdded = false;
    bool m_settingsDirty = false;  ///< Staged param edits not yet applied
    bool m_autoRegenerate = false; ///< Regenerate immediately on edit
};
