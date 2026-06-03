#pragma once

#include "terrain/TerrainData.h"

/// CPU-only procedural terrain generation utilities.
///
/// This module is intentionally renderer-independent: it does not create GPU
/// resources and can be used by a future TerrainScene, mesh builder, editor UI,
/// tests, or vegetation system without depending on OpenGL.
namespace TerrainGenerator
{
    /// Generates a deterministic heightfield from the provided settings.
    ///
    /// Layer stack:
    /// - macro base noise for broad continents/valleys
    /// - low-frequency region mask to decide where mountains/plateaus appear
    /// - rolling-hill fBM for readable mid-frequency landforms
    /// - masked ridged fBM for mountain ranges
    /// - plateau shaping to flatten selected high regions
    /// - small detail noise with low amplitude
    ///
    /// Same settings and seed always produce byte-identical sample heights.
    [[nodiscard]] TerrainHeightfield GenerateHeightfield(
        const TerrainGenerationSettings& settings,
        const TerrainClassificationSettings& classificationSettings = {});

    /// Recomputes normals, slope, material zone, and placement masks for an
    /// existing heightfield. Useful after a later erosion pass edits heights.
    void RebuildDerivedData(TerrainHeightfield& heightfield,
                            const TerrainClassificationSettings& classificationSettings = {});

    /// Loads a greyscale PNG file as a heightfield.  Auto-detects 16-bit vs 8-bit.
    /// Applies settings.heightmapGamma as a power curve, then calls RebuildDerivedData
    /// so normals, slopes, material zones, and suitability masks are identical to
    /// the procedural path.  Image dimensions override settings.gridWidth/Height.
    [[nodiscard]] TerrainHeightfield LoadHeightmapFromPNG(
        const std::string& path,
        const TerrainGenerationSettings& settings,
        const TerrainClassificationSettings& classificationSettings = {});
}
