#pragma once

#include "assets/ModelData.h"
#include "core/Material.h"
#include "terrain/TerrainData.h"

#include <glm/glm.hpp>
#include <memory>
#include <random>
#include <string>
#include <vector>

class ShaderProgram;

// ─── VegetationInstance ───────────────────────────────────────────────────────
// One placed prop instance with its bounding sphere for per-instance culling.

struct VegetationInstance
{
    glm::vec3 position;       ///< World-space origin (terrain surface Y)
    float     boundingRadius; ///< Approximate sphere radius for frustum cull
    glm::mat4 transform;      ///< Full world-space model matrix
};

// ─── VegetationGroup ─────────────────────────────────────────────────────────
// One loaded model with all its CPU-side instances and a GPU instance VBO.
// DrawSubMeshInstanced emits one instanced draw per submesh per frame.

class VegetationGroup
{
public:
    VegetationGroup()  = default;
    ~VegetationGroup();

    VegetationGroup(const VegetationGroup&)            = delete;
    VegetationGroup& operator=(const VegetationGroup&) = delete;
    VegetationGroup(VegetationGroup&&)                 = default;
    VegetationGroup& operator=(VegetationGroup&&)      = default;

    /// Load a glTF model and build per-material MaterialInstances.
    bool Load(const std::string& gltfPath,
              const std::shared_ptr<ShaderProgram>& shader);

    /// Add one instance at the given world position with scale, Y-rotation,
    /// and small X/Z tilts.  Thread-safe only if called from a single thread.
    void AddInstance(glm::vec3 worldPos, float scale,
                     float yRotDeg, float tiltXDeg, float tiltZDeg);

    void ClearInstances();

    /// Upload m_visibleTransforms to the instance VBO (grows as needed).
    void UploadInstances();

    /// Draw all visible instances.  Caller must have the Camera and Light UBOs
    /// already bound (they persist from Renderer::BeginFrame).
    void Draw() const;

    [[nodiscard]] bool   IsValid()       const { return m_model.IsValid(); }
    [[nodiscard]] size_t InstanceCount() const { return m_instances.size(); }
    [[nodiscard]] size_t VisibleCount()  const { return m_visibleCount; }

    // ── Settings (tweakable from ImGui) ───────────────────────────────────────
    std::string name;
    float defaultScale  = 1.0f;  ///< Base scale baked into transforms at placement time
    float scaleOverride = 1.0f;  ///< Live multiplier applied each frame in CullAndUpload — no regen needed
    float radiusFactor  = 5.0f;  ///< Bounding sphere = scale * radiusFactor
    float lodDistance   = 250.0f;///< Skip instances beyond this camera distance

    // ── State ─────────────────────────────────────────────────────────────────
    std::vector<VegetationInstance>                m_instances;
    std::vector<glm::mat4>                         m_visibleTransforms;
    size_t                                         m_visibleCount = 0;

    ModelData                                      m_model;
    std::shared_ptr<Material>                      m_baseMat;
    std::vector<std::unique_ptr<MaterialInstance>> m_matInstances;

    std::shared_ptr<ShaderProgram> m_shader;
    uint32_t m_instanceVbo         = 0; ///< Instanced mat4 vertex buffer (0 = not yet created)
    uint32_t m_instanceVboCapacity = 0; ///< Allocated mat4 slots
};

// ─── TerrainVegetation ───────────────────────────────────────────────────────
// Manages all vegetation groups for a terrain.
//
//   1. Setup()         — load models (once)
//   2. PlaceAll()      — run placement after each terrain generation
//   3. CullAndUpload() — per-frame: frustum cull + instance VBO upload
//   4. DrawAll()       — per-frame: one instanced draw per group

class TerrainVegetation
{
public:
    /// Load all configured model types.  Returns false if the instanced shader
    /// fails to load; groups whose models are absent are silently skipped.
    bool Setup(const std::shared_ptr<ShaderProgram>& shader);

    /// Place instances for all zones using the heightfield suitability masks.
    void PlaceAll(const TerrainHeightfield& hf, uint32_t seed);

    /// Clear all placed instances.
    void ClearAll();

    /// Per-frame: frustum-cull each group, then upload visible transforms.
    /// Pass the combined view-projection matrix and camera world position.
    void CullAndUpload(const glm::mat4& viewProj, const glm::vec3& cameraPos,
                       float verticalOffset = 0.0f);

    /// Per-frame: draw all groups with visible instances.
    void DrawAll() const;

    /// Draw ImGui controls for tuning placement and per-group settings.
    void OnImGui();

    [[nodiscard]] bool IsReady() const { return m_setupDone; }

    // ── Global placement settings ─────────────────────────────────────────────
    bool  m_enabled         = false;
    float m_density         = 1.0f;   ///< Global density multiplier (0.25..2)
    float m_lodDistanceMult = 1.0f;
    float m_treeMaskMin     = 0.28f;
    float m_grassMaskMin    = 0.28f;
    float m_rockMaskMin     = 0.32f;
    float m_sandHeightMax   = 0.15f;  ///< Normalised height ceiling for sand zone

    /// When true PlaceAll derives per-instance scale from world size, elevation,
    /// slope, and local roughness so vegetation always looks proportional to the
    /// terrain regardless of heightScale or worldWidth.
    bool  m_autoScale       = true;

private:
    struct SpatialGrid;             // Forward-declared — defined in .cpp

    void PlaceForestZone(const TerrainHeightfield& hf, std::mt19937& rng,
                         SpatialGrid& treeGrid, SpatialGrid& logGrid,
                         SpatialGrid& rockUnderstoryGrid);
    void PlaceGrassZone (const TerrainHeightfield& hf, std::mt19937& rng,
                         SpatialGrid& grassGrid, SpatialGrid& flowerGrid,
                         SpatialGrid& bushGrid);
    void PlaceRockZone  (const TerrainHeightfield& hf, std::mt19937& rng,
                         SpatialGrid& boulderGrid, SpatialGrid& rockGrid,
                         SpatialGrid& stoneGrid, SpatialGrid& pebblesGrid);
    void PlaceSandZone  (const TerrainHeightfield& hf, std::mt19937& rng,
                         SpatialGrid& cactusGrid, SpatialGrid& sandStoneGrid);

    void ScatterCluster(VegetationGroup& group,
                        glm::vec3 parentPos, int count, float radius,
                        float scaleMin, float scaleMax, float minSpacing,
                        std::mt19937& rng, SpatialGrid& grid,
                        const TerrainHeightfield& hf,
                        float baseScaleMult = 1.0f);

    // ── Forest ────────────────────────────────────────────────────────────────
    VegetationGroup m_realisticTree;
    VegetationGroup m_treesLowPoly;
    VegetationGroup m_treeGn;
    VegetationGroup m_stylizedLog;
    VegetationGroup m_simpleRock;

    // ── Grass ─────────────────────────────────────────────────────────────────
    VegetationGroup m_lowpolyGrass;
    VegetationGroup m_flower;
    VegetationGroup m_bush;

    // ── Rock ──────────────────────────────────────────────────────────────────
    VegetationGroup m_boulder;
    VegetationGroup m_rock;
    VegetationGroup m_stone;
    VegetationGroup m_pebbles;

    // ── Sand ──────────────────────────────────────────────────────────────────
    VegetationGroup m_cactus;
    VegetationGroup m_cactus2;

    // Stats shown in ImGui
    int m_statTrees   = 0;
    int m_statGrass   = 0;
    int m_statRock    = 0;
    int m_statSand    = 0;
    int m_statVisible = 0;

    bool  m_setupDone        = false;
    float m_worldScaleFactor = 1.0f; ///< Computed in PlaceAll(); applied per-instance
};
