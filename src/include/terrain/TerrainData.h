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

    // ── Macro landform shaping ────────────────────────────────────────────────
    float broadHillScale    = 0.0012f; ///< Spatial frequency of broad hill noise
    float broadHillStrength = 0.16f;   ///< Contribution strength of broad hill shaping [0..1]
    float valleyScale       = 0.0012f; ///< Spatial frequency of valley noise
    float valleyStrength    = 0.14f;   ///< Contribution strength of valley shaping [0..1]

    // ── Rolling hills layer ───────────────────────────────────────────────────
    float hillScale     = 0.004f;  ///< Spatial frequency of rolling hills
    float hillAmplitude = 0.45f;   ///< Contribution weight of hill layer [0..1]
    int   hillOctaves   = 4;       ///< fBM octaves for hill layer
    float hillPersistence = 0.5f;
    float hillLacunarity  = 2.0f;

    // ── Mountain region layer ─────────────────────────────────────────────────
    float mountainRegionMaskScale = 0.0006f; ///< Spatial frequency of mountain-region mask
    float mountainScale     = 0.003f; ///< Spatial frequency of mountain noise
    float mountainAmplitude = 0.85f;  ///< Peak contribution weight [0..1]
    int   mountainOctaves   = 6;
    float mountainPersistence = 0.5f;
    float mountainLacunarity  = 2.1f;
    float ridgeSharpness      = 2.0f; ///< Exponent for ridged-noise shaping
    float mountainRidgeScale       = 0.003f; ///< Spatial frequency of mountain ridge noise
    float mountainRidgeStrength    = 0.85f;  ///< Contribution strength of mountain ridges [0..1]
    int   mountainRidgeOctaves     = 6;      ///< fBM octaves for mountain ridge noise
    float mountainRidgePersistence = 0.5f;
    float mountainRidgeLacunarity  = 2.1f;

    // ── Plateau shaping ───────────────────────────────────────────────────────
    float plateauRegionScale     = 0.0006f; ///< Spatial frequency of plateau-region mask
    float plateauThreshold       = 0.62f;   ///< Normalized height above which terrain flattens
    float plateauStrength        = 0.35f;   ///< How strongly the plateau effect is applied [0..1]
    float plateauFlatteningAmount = 0.78f;  ///< Height variation removed above plateau threshold [0..1]

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

    // ── Volcano shaping ───────────────────────────────────────────────────────
    // Injects a radial bias term into the height stack that forms a cone
    // peaking at rimRadius with a caldera punch-down at the center.
    // Noise layers still run on top of this pedestal for organic irregularity.
    bool  volcanoEnabled           = false;
    float volcanoRimRadius         = 0.38f; ///< Normalised radius to rim peak (0=center, 1=world edge)
    float volcanoConeHeight        = 0.72f; ///< Height bias added at the rim peak
    float volcanoCalderaDepth      = 0.28f; ///< Depth of the caldera punch-down at the center
    float volcanoCalderaOuterRatio = 0.60f; ///< Outer caldera smoothstep edge as fraction of rimRadius
    float volcanoCalderaInnerRatio = 0.15f; ///< Inner caldera smoothstep edge as fraction of rimRadius

    // ── Domain warping ────────────────────────────────────────────────────────
    // Offsets noise sampling coordinates by a low-frequency noise field before
    // any height layers are evaluated.  Breaks the blobby regularity of value
    // noise and makes terrain read as wind-carved and geologically stressed.
    bool  domainWarpEnabled  = false;
    float domainWarpScale    = 0.003f; ///< Spatial frequency of the warp offset noise
    float domainWarpStrength = 30.0f;  ///< Maximum warp offset in world units
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

/// Alias used by later terrain systems (texturing/vegetation) to avoid leaking
/// the "Terrain" prefix everywhere.
using MaterialZone = TerrainMaterialZone;

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
    float steepExclusion   = 0.0f; ///< 1 = too steep for vegetation (legacy semantics)

    // ── Sprint 10 (Task 4) masks — values in [0..1] ──────────────────────────
    // These are the CPU-side metadata used for later vegetation placement and
    // texture splatting. "steepSlopeExclusion" is a plantable-ground gate:
    // 1 = safe/flat enough, 0 = too steep.
    float grassMask            = 0.0f;
    float treeMask             = 0.0f;
    float rockMask             = 0.0f;
    float steepSlopeExclusion  = 1.0f;

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
// TerrainClassificationSettings (Sprint 10, Task 4)
// All thresholds + blend widths used for height/slope material zoning and
// suitability masks. Designed to be tweakable later from ImGui.
// ─────────────────────────────────────────────────────────────────────────────

struct TerrainClassificationSettings
{
    // Height bands (h01 in [0..1])
    float deepWaterHeight   = 0.02f;
    float shallowWaterHeight = 0.06f;
    float sandHeight        = 0.10f;

    float grassMinStart     = 0.06f;
    float grassMinEnd       = 0.10f;
    float grassMaxStart     = 0.50f;
    float grassMaxEnd       = 0.60f;

    float forestMinStart    = 0.20f;
    float forestMinEnd      = 0.28f;
    float forestMaxStart    = 0.55f;
    float forestMaxEnd      = 0.72f;

    float mountainStart     = 0.70f;
    float mountainFull      = 0.88f;

    float snowStart         = 0.86f;
    float snowFull          = 0.94f;

    // Slope thresholds (slope01 in [0..1])
    float sandSlopeStart    = 0.10f;
    float sandSlopeEnd      = 0.25f;

    float grassSlopeStart   = 0.18f;
    float grassSlopeEnd     = 0.40f;

    float forestSlopeStart  = 0.22f;
    float forestSlopeEnd    = 0.55f;

    float rockSlopeStart    = 0.45f;
    float rockSlopeEnd      = 0.65f;

    // Snow doesn't stick to cliffs: above this slope, snow fades out to rock.
    float snowSlopeStart    = 0.25f;
    float snowSlopeEnd      = 0.45f;

    // Vegetation exclusion: 1 = plantable, 0 = too steep.
    float excludeSlopeStart = 0.45f;
    float excludeSlopeEnd   = 0.70f;

    // Trees: treeline and slope limits
    float treeMinStart      = 0.10f;
    float treeMinEnd        = 0.16f;
    float treeMaxStart      = 0.55f;
    float treeMaxEnd        = 0.72f;
    float treeSlopeStart    = 0.20f;
    float treeSlopeEnd      = 0.45f;

    // Rock scatter suitability
    float rockMaskHeightStart = 0.55f;
    float rockMaskHeightEnd   = 0.80f;
    float rockMaskSlopeStart  = 0.35f;
    float rockMaskSlopeEnd    = 0.65f;
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
