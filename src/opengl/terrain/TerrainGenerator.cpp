#include "terrain/TerrainGenerator.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
#include <glm/geometric.hpp>

namespace
{
constexpr float kInvUintMax = 1.0f / 4294967295.0f;
constexpr int   kRegionMaskOctaves = 2;

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

TerrainRegionMaskSample SampleTerrainRegionMasks(float wx, float wz, const TerrainGenerationSettings& settings)
{
    return {
        RegionMask(wx, wz, settings.broadHillScale, settings.seed + 401u),
        RegionMask(wx, wz, settings.valleyScale, settings.seed + 503u),
        RegionMask(wx, wz, settings.plateauRegionScale, settings.seed + 607u),
        RegionMask(wx, wz, settings.mountainRegionMaskScale, settings.seed + 709u)
    };
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

TerrainMaterialZone Classify(float h, float slope, const TerrainMaterialThresholds& t)
{
    if (h < t.deepWaterHeight) return TerrainMaterialZone::DeepWater;
    if (h < t.shallowWaterHeight) return TerrainMaterialZone::ShallowWater;
    if (h < t.sandHeight && slope < t.grassMaxSlope) return TerrainMaterialZone::Sand;
    if (slope >= t.rockMinSlope) return TerrainMaterialZone::Rock;
    if (h < t.grassMaxHeight && slope < t.grassMaxSlope) return TerrainMaterialZone::Grass;
    if (h < t.forestMaxHeight && slope < t.rockMinSlope) return TerrainMaterialZone::Forest;
    if (h < t.rockMaxHeight) return TerrainMaterialZone::Rock;
    return TerrainMaterialZone::Snow;
}
}

namespace TerrainGenerator
{
TerrainHeightfield GenerateHeightfield(const TerrainGenerationSettings& settings,
                                       const TerrainMaterialThresholds& thresholds)
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

            [[maybe_unused]] const TerrainRegionMaskSample& regionMask = regionMasks.At(x, z);

            const float macro = Fbm(wx * settings.macroScale, wz * settings.macroScale, settings.seed + 11u, 3, 0.55f, 2.0f);
            const float region = Fbm(wx * settings.regionMaskScale, wz * settings.regionMaskScale, settings.seed + 29u, 3, 0.6f, 2.0f);
            const float mountainMask = Smoothstep(0.48f, 0.78f, region);
            const float plateauMask = Smoothstep(0.58f, 0.86f, Fbm(wx * settings.regionMaskScale, wz * settings.regionMaskScale, settings.seed + 47u, 2, 0.55f, 2.0f));
            const float hills = Fbm(wx * settings.hillScale, wz * settings.hillScale, settings.seed + 101u,
                                    settings.hillOctaves, settings.hillPersistence, settings.hillLacunarity);
            const float ridges = RidgedFbm(wx * settings.mountainScale, wz * settings.mountainScale, settings.seed + 211u,
                                           settings.mountainOctaves, settings.mountainPersistence,
                                           settings.mountainLacunarity, settings.ridgeSharpness);
            const float detail = ValueNoise(wx * settings.detailScale, wz * settings.detailScale, settings.seed + 307u) - 0.5f;

            float h = 0.18f;
            h += (macro - 0.5f) * settings.macroAmplitude * 0.55f;
            h += hills * settings.hillAmplitude * (1.0f - mountainMask * 0.35f);
            h += ridges * settings.mountainAmplitude * mountainMask;
            h += detail * settings.detailAmplitude;
            h = Clamp01(h);

            if (plateauMask > 0.0f && h > settings.plateauThreshold)
            {
                const float flattened = settings.plateauThreshold + (h - settings.plateauThreshold) * 0.22f;
                h = Lerp(h, flattened, Clamp01(settings.plateauStrength * plateauMask));
            }

            TerrainSample& sample = hf.At(x, z);
            sample.normalizedHeight = Clamp01(h);
            sample.height = sample.normalizedHeight * settings.heightScale;
            sample.mountainMask = mountainMask;
            hf.minHeight = std::min(hf.minHeight, sample.height);
            hf.maxHeight = std::max(hf.maxHeight, sample.height);
        }
    }

    RebuildDerivedData(hf, thresholds);
    return hf;
}

void RebuildDerivedData(TerrainHeightfield& hf, const TerrainMaterialThresholds& thresholds)
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
            s.materialZone = Classify(s.normalizedHeight, s.slope, thresholds);
            s.steepExclusion = Smoothstep(thresholds.grassMaxSlope, thresholds.steepThreshold, s.slope);
            s.grassSuitability = Clamp01((1.0f - s.steepExclusion) * (1.0f - s.mountainMask) * Smoothstep(thresholds.sandHeight, thresholds.grassMaxHeight, s.normalizedHeight));
            s.treeSuitability = Clamp01((1.0f - s.steepExclusion) * (1.0f - s.mountainMask) * Smoothstep(0.18f, thresholds.forestMaxHeight, s.normalizedHeight) * (1.0f - Smoothstep(thresholds.forestMaxHeight, thresholds.rockMaxHeight, s.normalizedHeight)));
            s.rockSuitability = Clamp01(std::max(s.mountainMask, Smoothstep(thresholds.rockMinSlope * 0.65f, thresholds.rockMinSlope, s.slope)));
        }
    }
}
}
