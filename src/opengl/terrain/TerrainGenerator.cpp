#include "terrain/TerrainGenerator.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>
#include <stb_image.h>

namespace
{
constexpr float kInvUintMax = 1.0f / 4294967295.0f;
constexpr int   kRegionMaskOctaves = 2;
constexpr int   kMacroShapeOctaves = 2;

struct TerrainRegionMaskSample
{
    float broadHills = 0.0f;
    float valleys    = 0.0f;
    float plateaus   = 0.0f;
    float mountains  = 0.0f;
};

struct TerrainRegionMasks
{
    uint32_t width  = 0;
    uint32_t height = 0;
    std::vector<TerrainRegionMaskSample> samples;

    [[nodiscard]] const TerrainRegionMaskSample& At(uint32_t x, uint32_t z) const
    {
        return samples[z * width + x];
    }
};

float Clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }
float Smoothstep(float edge0, float edge1, float x)
{
    const float t = Clamp01((x - edge0) / (edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}
float Lerp(float a, float b, float t) { return a + (b - a) * t; }

uint32_t Hash(uint32_t x, uint32_t y, uint32_t seed)
{
    uint32_t h = seed ^ (x * 0x9E3779B9u) ^ (y * 0x85EBCA6Bu);
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

float ValueAt(int x, int y, uint32_t seed)
{
    return static_cast<float>(Hash(static_cast<uint32_t>(x), static_cast<uint32_t>(y), seed)) * kInvUintMax;
}

float ValueNoise(float x, float y, uint32_t seed)
{
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = x0 + 1;
    const int y1 = y0 + 1;
    const float tx = x - static_cast<float>(x0);
    const float ty = y - static_cast<float>(y0);
    const float sx = tx * tx * (3.0f - 2.0f * tx);
    const float sy = ty * ty * (3.0f - 2.0f * ty);

    const float a = Lerp(ValueAt(x0, y0, seed), ValueAt(x1, y0, seed), sx);
    const float b = Lerp(ValueAt(x0, y1, seed), ValueAt(x1, y1, seed), sx);
    return Lerp(a, b, sy);
}

float Fbm(float x, float y, uint32_t seed, int octaves, float persistence, float lacunarity)
{
    float value = 0.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float norm = 0.0f;
    for (int i = 0; i < std::max(1, octaves); ++i)
    {
        value += ValueNoise(x * frequency, y * frequency, seed + static_cast<uint32_t>(i) * 1013u) * amplitude;
        norm += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }
    return norm > 0.0f ? value / norm : 0.0f;
}

float RegionMask(float wx, float wz, float scale, uint32_t seed)
{
    const float n = Fbm(wx * scale, wz * scale, seed, kRegionMaskOctaves, 0.55f, 2.0f);
    return Smoothstep(0.25f, 0.75f, n);
}

TerrainRegionMaskSample ComposeTerrainRegionMasks(const TerrainRegionMaskSample& raw)
{
    TerrainRegionMaskSample masks;
    masks.mountains = Clamp01(raw.mountains);
    masks.plateaus = Clamp01(raw.plateaus * (1.0f - masks.mountains * 0.65f));
    masks.valleys = Clamp01(raw.valleys * (1.0f - masks.mountains * 0.75f) *
                            (1.0f - masks.plateaus * 0.55f));
    masks.broadHills = Clamp01(raw.broadHills * (1.0f - masks.valleys * 0.85f) *
                               (1.0f - masks.plateaus * 0.45f) *
                               (1.0f - masks.mountains * 0.85f));
    return masks;
}

float MacroShapeVariation(float wx, float wz, float scale, uint32_t seed)
{
    const float n = Fbm(wx * scale, wz * scale, seed, kMacroShapeOctaves, 0.55f, 2.0f);
    return Smoothstep(0.20f, 0.85f, n);
}

float PlateauTargetHeight(float wx, float wz, const TerrainGenerationSettings& settings)
{
    const float n = Fbm(wx * settings.plateauRegionScale, wz * settings.plateauRegionScale,
                        settings.seed + 1009u, kMacroShapeOctaves, 0.55f, 2.0f);
    return Lerp(settings.plateauThreshold, 0.86f, Smoothstep(0.20f, 0.85f, n));
}

float ApplyPlateauShaping(float h, float wx, float wz, const TerrainRegionMaskSample& regionMask,
                          const TerrainGenerationSettings& settings)
{
    const float plateauBlend = regionMask.plateaus * Clamp01(settings.plateauStrength);
    const float flattening = Clamp01(settings.plateauFlatteningAmount);
    const float targetHeight = PlateauTargetHeight(wx, wz, settings);
    const float flattened = targetHeight + (h - targetHeight) * (1.0f - flattening);
    return Lerp(h, flattened, plateauBlend);
}

TerrainRegionMaskSample SampleTerrainRegionMasks(float wx, float wz, const TerrainGenerationSettings& settings)
{
    const TerrainRegionMaskSample rawMasks = {
        RegionMask(wx, wz, settings.broadHillScale, settings.seed + 401u),
        RegionMask(wx, wz, settings.valleyScale, settings.seed + 503u),
        RegionMask(wx, wz, settings.plateauRegionScale, settings.seed + 607u),
        RegionMask(wx, wz, settings.mountainRegionMaskScale, settings.seed + 709u)
    };
    return ComposeTerrainRegionMasks(rawMasks);
}

TerrainRegionMasks BuildTerrainRegionMasks(const TerrainGenerationSettings& settings, uint32_t width, uint32_t height)
{
    TerrainRegionMasks masks;
    masks.width = width;
    masks.height = height;
    masks.samples.resize(static_cast<size_t>(width) * height);

    for (uint32_t z = 0; z < height; ++z)
    {
        for (uint32_t x = 0; x < width; ++x)
        {
            const float wx = (static_cast<float>(x) / static_cast<float>(width - 1) - 0.5f) * settings.worldWidth;
            const float wz = (static_cast<float>(z) / static_cast<float>(height - 1) - 0.5f) * settings.worldHeight;
            masks.samples[z * width + x] = SampleTerrainRegionMasks(wx, wz, settings);
        }
    }

    return masks;
}

float RidgedFbm(float x, float y, uint32_t seed, int octaves, float persistence, float lacunarity, float sharpness)
{
    float value = 0.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float norm = 0.0f;
    for (int i = 0; i < std::max(1, octaves); ++i)
    {
        const float n = Fbm(x * frequency, y * frequency, seed + static_cast<uint32_t>(i) * 9176u, 1, persistence, lacunarity);
        value += std::pow(1.0f - std::abs(n * 2.0f - 1.0f), std::max(0.1f, sharpness)) * amplitude;
        norm += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }
    return norm > 0.0f ? value / norm : 0.0f;
}

float MountainRidgeVariation(float wx, float wz, const TerrainGenerationSettings& settings)
{
    return RidgedFbm(wx * settings.mountainRidgeScale, wz * settings.mountainRidgeScale,
                     settings.seed + 1217u, settings.mountainRidgeOctaves,
                     settings.mountainRidgePersistence, settings.mountainRidgeLacunarity,
                     settings.ridgeSharpness);
}

TerrainMaterialZone ChooseDominantZone(float deepWaterW,
                                       float shallowWaterW,
                                       float sandW,
                                       float grassW,
                                       float forestW,
                                       float rockW,
                                       float snowW)
{
    TerrainMaterialZone zone = TerrainMaterialZone::Grass;
    float bestW = grassW;

    auto consider = [&](TerrainMaterialZone z, float w)
    {
        if (w > bestW)
        {
            bestW = w;
            zone = z;
        }
    };

    consider(TerrainMaterialZone::DeepWater, deepWaterW);
    consider(TerrainMaterialZone::ShallowWater, shallowWaterW);
    consider(TerrainMaterialZone::Sand, sandW);
    consider(TerrainMaterialZone::Forest, forestW);
    consider(TerrainMaterialZone::Rock, rockW);
    consider(TerrainMaterialZone::Snow, snowW);
    return zone;
}
}

namespace TerrainGenerator
{
TerrainHeightfield GenerateHeightfield(const TerrainGenerationSettings& settings,
                                       const TerrainClassificationSettings& classificationSettings)
{
    TerrainHeightfield hf;
    hf.width = std::max(2u, settings.gridWidth);
    hf.height = std::max(2u, settings.gridHeight);
    hf.settings = settings;
    hf.samples.resize(static_cast<size_t>(hf.width) * hf.height);
    hf.minHeight = std::numeric_limits<float>::max();
    hf.maxHeight = std::numeric_limits<float>::lowest();

    const TerrainRegionMasks regionMasks = BuildTerrainRegionMasks(settings, hf.width, hf.height);

    for (uint32_t z = 0; z < hf.height; ++z)
    {
        for (uint32_t x = 0; x < hf.width; ++x)
        {
            const float wx = (static_cast<float>(x) / static_cast<float>(hf.width - 1) - 0.5f) * settings.worldWidth;
            const float wz = (static_cast<float>(z) / static_cast<float>(hf.height - 1) - 0.5f) * settings.worldHeight;

            // ── Domain warp ────────────────────────────────────────────────────
            // Region masks stay at (wx,wz) — warping them corrupts geographic
            // structure. Radial volcano distance also stays at (wx,wz).
            float wwx = wx, wwz = wz;
            if (settings.domainWarpEnabled)
            {
                const float s0 = settings.domainWarpScale;
                wwx += (ValueNoise(wx * s0, wz * s0, settings.seed + 3001u) - 0.5f) * settings.domainWarpStrength;
                wwz += (ValueNoise(wx * s0, wz * s0, settings.seed + 3002u) - 0.5f) * settings.domainWarpStrength;
            }

            const TerrainRegionMaskSample& regionMask = regionMasks.At(x, z);

            const float mountainMask = regionMask.mountains;
            const float macro = Fbm(wwx * settings.macroScale, wwz * settings.macroScale, settings.seed + 11u, 3, 0.55f, 2.0f);
            const float hills = Fbm(wwx * settings.hillScale, wwz * settings.hillScale, settings.seed + 101u,
                                    settings.hillOctaves, settings.hillPersistence, settings.hillLacunarity);
            const float mountainRidges = MountainRidgeVariation(wwx, wwz, settings);
            const float detail = ValueNoise(wwx * settings.detailScale, wwz * settings.detailScale, settings.seed + 307u) - 0.5f;
            const float broadHillShape = MacroShapeVariation(wwx, wwz, settings.broadHillScale, settings.seed + 811u);
            const float valleyShape = MacroShapeVariation(wwx, wwz, settings.valleyScale, settings.seed + 907u);

            float h = 0.18f;
            h += (macro - 0.5f) * settings.macroAmplitude * 0.55f;
            h += broadHillShape * Clamp01(settings.broadHillStrength) * regionMask.broadHills;
            h -= valleyShape * Clamp01(settings.valleyStrength) * regionMask.valleys;
            h += hills * settings.hillAmplitude * (1.0f - mountainMask * 0.35f);
            h += mountainRidges * Clamp01(settings.mountainRidgeStrength) * mountainMask;
            h += detail * settings.detailAmplitude;

            // ── Volcano radial bias ────────────────────────────────────────────
            // Sampled from (wx,wz) so the cone stays centered regardless of warp.
            // The noise stack above rides on top of this pedestal.
            if (settings.volcanoEnabled)
            {
                const float r    = glm::length(glm::vec2(wx, wz)) / (settings.worldWidth * 0.5f);
                const float rimR = settings.volcanoRimRadius;
                const float cone    = std::max(0.0f, 1.0f - std::abs(r - rimR) / rimR) * settings.volcanoConeHeight;
                const float caldera = Smoothstep(rimR * settings.volcanoCalderaOuterRatio,
                                                 rimR * settings.volcanoCalderaInnerRatio,
                                                 r) * settings.volcanoCalderaDepth;
                h += cone - caldera;
            }

            h = Clamp01(h);

            h = Clamp01(ApplyPlateauShaping(h, wwx, wwz, regionMask, settings));

            TerrainSample& sample = hf.At(x, z);
            sample.normalizedHeight = Clamp01(h);
            sample.height = sample.normalizedHeight * settings.heightScale;
            sample.mountainMask = mountainMask;
            hf.minHeight = std::min(hf.minHeight, sample.height);
            hf.maxHeight = std::max(hf.maxHeight, sample.height);
        }
    }

    RebuildDerivedData(hf, classificationSettings);
    return hf;
}

void RebuildDerivedData(TerrainHeightfield& hf, const TerrainClassificationSettings& c)
{
    if (!hf.IsValid()) return;
    const float dx = hf.settings.worldWidth / static_cast<float>(hf.width - 1);
    const float dz = hf.settings.worldHeight / static_cast<float>(hf.height - 1);

    for (uint32_t z = 0; z < hf.height; ++z)
    {
        for (uint32_t x = 0; x < hf.width; ++x)
        {
            const uint32_t xl = x > 0 ? x - 1 : x;
            const uint32_t xr = x + 1 < hf.width ? x + 1 : x;
            const uint32_t zd = z > 0 ? z - 1 : z;
            const uint32_t zu = z + 1 < hf.height ? z + 1 : z;
            const float dhdx = (hf.At(xr, z).height - hf.At(xl, z).height) / (static_cast<float>(xr - xl) * dx);
            const float dhdz = (hf.At(x, zu).height - hf.At(x, zd).height) / (static_cast<float>(zu - zd) * dz);

            TerrainSample& s = hf.At(x, z);
            s.normal = glm::normalize(glm::vec3(-dhdx, 1.0f, -dhdz));
            s.slope = Clamp01(1.0f - s.normal.y);

            // Height and slope are already normalized deterministically:
            // - normalizedHeight comes from the generator stack (0..1)
            // - slope is derived from the world-space gradient (dx/dz)
            const float h01 = Clamp01(s.normalizedHeight);
            const float slope01 = Clamp01(s.slope);

            // ── Material zone weights (smooth transitions) ─────────────────────
            const float deepWaterW = 1.0f - Smoothstep(c.deepWaterHeight, c.shallowWaterHeight, h01);
            const float shallowWaterW =
                Smoothstep(c.deepWaterHeight, c.shallowWaterHeight, h01) *
                (1.0f - Smoothstep(c.shallowWaterHeight, c.sandHeight, h01));

            const float sandHeightW = Smoothstep(c.shallowWaterHeight, c.sandHeight, h01) *
                                      (1.0f - Smoothstep(c.sandHeight, c.grassMinEnd, h01));
            const float sandSlopeW = 1.0f - Smoothstep(c.sandSlopeStart, c.sandSlopeEnd, slope01);
            const float sandW = Clamp01(sandHeightW * sandSlopeW);

            const float grassHeightW =
                Smoothstep(c.grassMinStart, c.grassMinEnd, h01) *
                (1.0f - Smoothstep(c.grassMaxStart, c.grassMaxEnd, h01));
            const float grassSlopeW = 1.0f - Smoothstep(c.grassSlopeStart, c.grassSlopeEnd, slope01);
            const float grassW = Clamp01(grassHeightW * grassSlopeW);

            const float forestHeightW =
                Smoothstep(c.forestMinStart, c.forestMinEnd, h01) *
                (1.0f - Smoothstep(c.forestMaxStart, c.forestMaxEnd, h01));
            const float forestSlopeW = 1.0f - Smoothstep(c.forestSlopeStart, c.forestSlopeEnd, slope01);
            const float forestW = Clamp01(forestHeightW * forestSlopeW);

            const float rockBySlopeW = Smoothstep(c.rockSlopeStart, c.rockSlopeEnd, slope01);
            const float rockByHeightW = Smoothstep(c.forestMaxStart, c.mountainStart, h01);
            const float rockW = Clamp01(std::max(rockBySlopeW, rockByHeightW));

            const float snowHeightW = Smoothstep(c.snowStart, c.snowFull, h01);
            const float snowStickW = 1.0f - Smoothstep(c.snowSlopeStart, c.snowSlopeEnd, slope01);
            const float snowW = Clamp01(snowHeightW * snowStickW);

            // Choose dominant zone for debug coloring / future texturing.
            s.materialZone = ChooseDominantZone(deepWaterW, shallowWaterW, sandW, grassW, forestW, rockW, snowW);

            // ── Suitability masks (Sprint 10, Task 4) ──────────────────────────
            // steepSlopeExclusion: 1 on plantable ground, 0 on steep slopes.
            s.steepSlopeExclusion = Clamp01(1.0f - Smoothstep(c.excludeSlopeStart, c.excludeSlopeEnd, slope01));

            // Mountain mask: keep the existing region-based mountain signal, and
            // reinforce with a height-derived component for predictable treeline.
            const float mountainByHeight = Smoothstep(c.mountainStart, c.mountainFull, h01);
            s.mountainMask = Clamp01(std::max(s.mountainMask, mountainByHeight));

            // Grass mask: low-mid heights + gentle slopes, multiplied by plantable gate.
            const float grassMask =
                (1.0f - Smoothstep(c.grassMaxStart, c.grassMaxEnd, h01)) *
                Smoothstep(c.grassMinStart, c.grassMinEnd, h01) *
                (1.0f - Smoothstep(c.grassSlopeStart, c.grassSlopeEnd, slope01)) *
                s.steepSlopeExclusion;
            s.grassMask = Clamp01(grassMask);

            // Tree mask: bounded by treeline and max slope, also multiplied by plantable gate.
            const float treeHeightW =
                Smoothstep(c.treeMinStart, c.treeMinEnd, h01) *
                (1.0f - Smoothstep(c.treeMaxStart, c.treeMaxEnd, h01));
            const float treeSlopeW = 1.0f - Smoothstep(c.treeSlopeStart, c.treeSlopeEnd, slope01);
            s.treeMask = Clamp01(treeHeightW * treeSlopeW * s.steepSlopeExclusion);

            // Rock scatter mask: prefers rocky slopes / higher regions where vegetation is sparse.
            const float rockHeightMaskW = Smoothstep(c.rockMaskHeightStart, c.rockMaskHeightEnd, h01);
            const float rockSlopeMaskW = Smoothstep(c.rockMaskSlopeStart, c.rockMaskSlopeEnd, slope01);
            s.rockMask = Clamp01(std::max(rockHeightMaskW, rockSlopeMaskW) * (1.0f - s.grassMask) * (1.0f - s.treeMask));

            // Maintain legacy fields for existing code paths (mesh packing / debug)
            // while keeping the new Task 4 semantics explicit.
            s.grassSuitability = s.grassMask;
            s.treeSuitability  = s.treeMask;
            s.rockSuitability  = s.rockMask;
            s.steepExclusion   = Clamp01(1.0f - s.steepSlopeExclusion);
        }
    }
}

// ─── LoadHeightmapFromPNG ─────────────────────────────────────────────────────

TerrainHeightfield LoadHeightmapFromPNG(
    const std::string& path,
    const TerrainGenerationSettings& settings,
    const TerrainClassificationSettings& classificationSettings)
{
    int w = 0, h = 0, ch = 0;

    // Prefer 16-bit for higher precision; fall back to 8-bit.
    stbi_us* px16 = stbi_load_16(path.c_str(), &w, &h, &ch, 1);
    uint8_t* px8  = nullptr;
    const bool is16 = (px16 != nullptr);

    if (!is16)
    {
        px8 = stbi_load(path.c_str(), &w, &h, &ch, 1);
        if (!px8)
        {
            spdlog::error("[TerrainGenerator] LoadHeightmapFromPNG: failed to load '{}'", path);
            return {};
        }
    }

    spdlog::info("[TerrainGenerator] Loaded {}x{} {} heightmap: {}",
                 w, h, is16 ? "16-bit" : "8-bit", path);

    TerrainHeightfield hf;
    hf.settings = settings;
    hf.width    = static_cast<uint32_t>(w);
    hf.height   = static_cast<uint32_t>(h);
    hf.samples.resize(static_cast<size_t>(w) * h);
    hf.minHeight =  std::numeric_limits<float>::max();
    hf.maxHeight = -std::numeric_limits<float>::max();

    for (int z = 0; z < h; ++z)
    {
        const int srcZ = settings.heightmapFlipY ? (h - 1 - z) : z;
        for (int x = 0; x < w; ++x)
        {
            const size_t srcIdx = static_cast<size_t>(srcZ) * w + x;
            float norm = is16
                ? static_cast<float>(px16[srcIdx]) / 65535.0f
                : static_cast<float>(px8[srcIdx])  / 255.0f;

            if (settings.heightmapGamma != 1.0f)
                norm = std::pow(norm, settings.heightmapGamma);

            TerrainSample& s   = hf.At(static_cast<uint32_t>(x), static_cast<uint32_t>(z));
            s.normalizedHeight = norm;
            s.height           = norm * settings.heightScale;

            if (s.height < hf.minHeight) hf.minHeight = s.height;
            if (s.height > hf.maxHeight) hf.maxHeight = s.height;
        }
    }

    if (is16) stbi_image_free(px16);
    else      stbi_image_free(px8);

    RebuildDerivedData(hf, classificationSettings);
    return hf;
}
}
