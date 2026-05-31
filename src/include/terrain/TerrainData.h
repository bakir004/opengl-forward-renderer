#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// TerrainGenerationSettings
// All parameters that drive procedural terrain generation.
// Passed to the generator; changing any field requires a regeneration pass.
// ─────────────────────────────────────────────────────────────────────────────

struct TerrainGenerationSettings
{
    // Grid resolution
    uint32_t gridWidth  = 256; ///< Number of vertices along X
    uint32_t gridHeight = 256; ///< Number of vertices along Z

    // World-space extents
    float worldWidth  = 512.0f; ///< Total world-space size along X
    float worldHeight = 512.0f; ///< Total world-space size along Z

    // Overall height scale
    float heightScale = 80.0f; ///< Maximum world-space terrain height

    // Seed
    uint32_t seed = 42; ///< Deterministic seed; same seed → identical terrain

    // ── Macro base shape ─────────────────────────────────────────────────────
    float macroScale     = 0.0012f; ///< Spatial frequency of broad landform noise
    float macroAmplitude = 1.0f;    ///< Contribution weight of macro layer [0..1]

    // ── Rolling hills layer ───────────────────────────────────────────────────
    float hillScale     = 0.004f;  ///< Spatial frequency of rolling hills
    float hillAmplitude = 0.45f;   ///< Contribution weight of hill layer [0..1]
    int   hillOctaves   = 4;       ///< fBM octaves for hill layer
    float hillPersistence = 0.5f;
    float hillLacunarity  = 2.0f;

    // ── Mountain region layer ─────────────────────────────────────────────────
    float mountainScale     = 0.003f; ///< Spatial frequency of mountain noise
    float mountainAmplitude = 0.85f;  ///< Peak contribution weight [0..1]
    int   mountainOctaves   = 6;
    float mountainPersistence = 0.5f;
    float mountainLacunarity  = 2.1f;
    float ridgeSharpness      = 2.0f; ///< Exponent for ridged-noise shaping

    // ── Plateau shaping ───────────────────────────────────────────────────────
    float plateauThreshold = 0.62f; ///< Normalized height above which terrain flattens
    float plateauStrength  = 0.35f; ///< How strongly the plateau effect is applied [0..1]

    // ── Small detail layer ────────────────────────────────────────────────────
    float detailScale     = 0.025f; ///< Spatial frequency of fine-grained detail
    float detailAmplitude = 0.06f;  ///< Contribution weight [0..1] — intentionally small

    // ── Low-frequency region masks ────────────────────────────────────────────
    float regionMaskScale = 0.0006f; ///< Spatial frequency of region-blending mask

    // ── Optional erosion post-process ────────────────────────────────────────
    bool  erosionEnabled    = false;
    int   erosionIterations = 50000;
    float erosionInertia    = 0.05f;
    float erosionCapacity   = 4.0f;
    float erosionDeposition = 0.1f;
    float erosionErosion    = 0.3f;
    float erosionEvaporation = 0.01f;
    float erosionMinSlope    = 0.01f;
};

// ─────────────────────────────────────────────────────────────────────────────
// TerrainMaterialZone
// Height/slope-based zone classification for a terrain sample.
// Used by the shader and by vegetation placement systems.
// ─────────────────────────────────────────────────────────────────────────────

enum class TerrainMaterialZone : uint8_t
{
    DeepWater  = 0, ///< Below sea level
    ShallowWater,   ///< Near sea level
    Sand,           ///< Low, flat coastal/beach areas
    Grass,          ///< Mid-elevation, gentle slope
    Forest,         ///< Mid-elevation, moderate slope
    Rock,           ///< High elevation or steep slope
    Snow,           ///< Highest elevations
    Count
};

// ─────────────────────────────────────────────────────────────────────────────
// TerrainSample
// Per-vertex data produced by the terrain generator.
// ─────────────────────────────────────────────────────────────────────────────

struct TerrainSample
{
    float height        = 0.0f; ///< World-space Y height
    float normalizedHeight = 0.0f; ///< Height in [0..1] relative to heightScale

    glm::vec3 normal    = {0.0f, 1.0f, 0.0f}; ///< Geometric surface normal
    float     slope     = 0.0f; ///< Slope in [0..1] (0=flat, 1=vertical)

    TerrainMaterialZone materialZone = TerrainMaterialZone::Grass;

    // ── Suitability masks — values in [0..1] ─────────────────────────────────
    float mountainMask     = 0.0f; ///< 1 = strongly in a mountain region
    float grassSuitability = 0.0f; ///< Suitability for grass coverage
    float treeSuitability  = 0.0f; ///< Suitability for tree placement
    float rockSuitability  = 0.0f; ///< Suitability for rock scatter
    float steepExclusion   = 0.0f; ///< 1 = too steep for vegetation

    // ── Optional erosion debug masks ─────────────────────────────────────────
    float erosionAmount    = 0.0f; ///< How much material was eroded here
    float depositionAmount = 0.0f; ///< How much material was deposited here
};

// ─────────────────────────────────────────────────────────────────────────────
// TerrainHeightfield
// The flat 2-D array of TerrainSamples produced by the generator.
// Row-major: index = z * width + x
// ─────────────────────────────────────────────────────────────────────────────

struct TerrainHeightfield
{
    uint32_t width  = 0; ///< Number of samples along X (== settings.gridWidth)
    uint32_t height = 0; ///< Number of samples along Z (== settings.gridHeight)

    std::vector<TerrainSample> samples; ///< width * height entries, row-major

    TerrainGenerationSettings settings; ///< Settings used to produce this heightfield

    // ── Derived bounds (filled by generator) ─────────────────────────────────
    float minHeight = 0.0f;
    float maxHeight = 0.0f;

    [[nodiscard]] bool IsValid() const
    {
        return width > 0 && height > 0 &&
               samples.size() == static_cast<size_t>(width) * height;
    }

    /// Returns a mutable reference to the sample at grid position (x, z).
    [[nodiscard]] TerrainSample& At(uint32_t x, uint32_t z)
    {
        return samples[z * width + x];
    }

    /// Returns a const reference to the sample at grid position (x, z).
    [[nodiscard]] const TerrainSample& At(uint32_t x, uint32_t z) const
    {
        return samples[z * width + x];
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// TerrainMeshData
// CPU-side mesh ready for GPU upload.
// Mirrors the MeshData convention used by the rest of the renderer
// (positions, normals, tangents, UVs, indices, bounds) but is terrain-specific
// so it can carry extra per-vertex data without polluting MeshData.
// ─────────────────────────────────────────────────────────────────────────────

struct TerrainVertex
{
    glm::vec3 position  = {0.0f, 0.0f, 0.0f};
    glm::vec3 normal    = {0.0f, 1.0f, 0.0f};
    glm::vec2 uv        = {0.0f, 0.0f};
    glm::vec4 tangent   = {1.0f, 0.0f, 0.0f, 1.0f}; ///< xyz=tangent, w=handedness

    // Per-vertex material / mask data packed for the GPU
    float     materialZone    = 0.0f; ///< TerrainMaterialZone cast to float
    float     mountainMask    = 0.0f;
    float     grassSuitability = 0.0f;
    float     treeSuitability  = 0.0f;
};

struct TerrainBounds
{
    glm::vec3 min = {0.0f, 0.0f, 0.0f};
    glm::vec3 max = {0.0f, 0.0f, 0.0f};

    [[nodiscard]] glm::vec3 Center() const { return (min + max) * 0.5f; }
    [[nodiscard]] glm::vec3 Extents() const { return (max - min) * 0.5f; }
};

struct TerrainMeshData
{
    std::vector<TerrainVertex> vertices;
    std::vector<uint32_t>      indices;
    TerrainBounds              bounds;

    [[nodiscard]] bool IsValid() const
    {
        return !vertices.empty() && !indices.empty();
    }

    [[nodiscard]] uint32_t VertexCount() const
    {
        return static_cast<uint32_t>(vertices.size());
    }

    [[nodiscard]] uint32_t IndexCount() const
    {
        return static_cast<uint32_t>(indices.size());
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// TerrainMaterialThresholds
// Tunable thresholds used by the material zone classifier.
// Exposed to the ImGui debug panel so they can be tweaked at runtime.
// ─────────────────────────────────────────────────────────────────────────────

struct TerrainMaterialThresholds
{
    float deepWaterHeight    = 0.02f;
    float shallowWaterHeight = 0.06f;
    float sandHeight         = 0.10f;
    float grassMaxHeight     = 0.55f;
    float forestMaxHeight    = 0.70f;
    float rockMaxHeight      = 0.88f;
    // Above rockMaxHeight → Snow

    float grassMaxSlope  = 0.30f; ///< Steeper than this → Rock instead of Grass/Forest
    float rockMinSlope   = 0.55f; ///< Above this slope, force Rock regardless of height
    float steepThreshold = 0.70f; ///< Above this slope, set steepExclusion = 1
};

// ─────────────────────────────────────────────────────────────────────────────
// TerrainStats
// Runtime statistics reported by the generator and mesh builder.
// Surfaced in the debug UI.
// ─────────────────────────────────────────────────────────────────────────────

struct TerrainStats
{
    float generationTimeMs  = 0.0f; ///< CPU time to generate the heightfield
    float meshBuildTimeMs   = 0.0f; ///< CPU time to build TerrainMeshData
    float meshUploadTimeMs  = 0.0f; ///< GPU upload time
    uint32_t vertexCount    = 0;
    uint32_t indexCount     = 0;
    uint32_t triangleCount  = 0;
    std::string lastSeedStr;        ///< Human-readable seed used
};