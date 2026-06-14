#include "terrain/TerrainGenerator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <random>
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

float SampleHeightfield(const TerrainHeightfield& hf, float fx, float fz)
{
    fx = std::clamp(fx, 0.0f, static_cast<float>(hf.width - 1));
    fz = std::clamp(fz, 0.0f, static_cast<float>(hf.height - 1));

    const uint32_t x0 = static_cast<uint32_t>(std::floor(fx));
    const uint32_t z0 = static_cast<uint32_t>(std::floor(fz));
    const uint32_t x1 = std::min(x0 + 1, hf.width - 1);
    const uint32_t z1 = std::min(z0 + 1, hf.height - 1);
    const float sx = fx - static_cast<float>(x0);
    const float sz = fz - static_cast<float>(z0);

    const float h00 = hf.At(x0, z0).height;
    const float h10 = hf.At(x1, z0).height;
    const float h01 = hf.At(x0, z1).height;
    const float h11 = hf.At(x1, z1).height;

    const float hx0 = Lerp(h00, h10, sx);
    const float hx1 = Lerp(h01, h11, sx);
    return Lerp(hx0, hx1, sz);
}

float SampleHeightfield(const std::vector<float>& heights,
                        uint32_t width,
                        uint32_t height,
                        float fx,
                        float fz)
{
    fx = std::clamp(fx, 0.0f, static_cast<float>(width - 1));
    fz = std::clamp(fz, 0.0f, static_cast<float>(height - 1));

    const uint32_t x0 = static_cast<uint32_t>(std::floor(fx));
    const uint32_t z0 = static_cast<uint32_t>(std::floor(fz));
    const uint32_t x1 = std::min(x0 + 1, width - 1);
    const uint32_t z1 = std::min(z0 + 1, height - 1);
    const float sx = fx - static_cast<float>(x0);
    const float sz = fz - static_cast<float>(z0);

    const float h00 = heights[z0 * width + x0];
    const float h10 = heights[z0 * width + x1];
    const float h01 = heights[z1 * width + x0];
    const float h11 = heights[z1 * width + x1];

    const float hx0 = Lerp(h00, h10, sx);
    const float hx1 = Lerp(h01, h11, sx);
    return Lerp(hx0, hx1, sz);
}

glm::vec2 SampleHeightfieldGradient(const std::vector<float>& heights,
                                    uint32_t width,
                                    uint32_t height,
                                    float fx,
                                    float fz)
{
    fx = std::clamp(fx, 0.0f, static_cast<float>(width - 1));
    fz = std::clamp(fz, 0.0f, static_cast<float>(height - 1));

    const uint32_t x0 = static_cast<uint32_t>(std::floor(fx));
    const uint32_t z0 = static_cast<uint32_t>(std::floor(fz));
    const uint32_t x1 = std::min(x0 + 1, width - 1);
    const uint32_t z1 = std::min(z0 + 1, height - 1);
    const float sx = fx - static_cast<float>(x0);
    const float sz = fz - static_cast<float>(z0);

    const float h00 = heights[z0 * width + x0];
    const float h10 = heights[z0 * width + x1];
    const float h01 = heights[z1 * width + x0];
    const float h11 = heights[z1 * width + x1];

    const float dhdx = ((1.0f - sz) * (h10 - h00) + sz * (h11 - h01));
    const float dhdz = ((1.0f - sx) * (h01 - h00) + sx * (h11 - h10));
    return {dhdx, dhdz};
}

void UpdateHeightBounds(TerrainHeightfield& hf)
{
    hf.minHeight = std::numeric_limits<float>::max();
    hf.maxHeight = std::numeric_limits<float>::lowest();
    for (const TerrainSample& sample : hf.samples)
    {
        hf.minHeight = std::min(hf.minHeight, sample.height);
        hf.maxHeight = std::max(hf.maxHeight, sample.height);
    }
}

void GaussianSmoothHeightfield(TerrainHeightfield& hf, int passes, int radius, float strength)
{
    if (!hf.IsValid() || passes <= 0 || radius <= 0 || strength <= 0.0f)
        return;

    const uint32_t width = hf.width;
    const uint32_t height = hf.height;
    const size_t sampleCount = static_cast<size_t>(width) * height;
    const float blend = std::clamp(strength, 0.0f, 1.0f);

    std::vector<float> source(sampleCount);
    std::vector<float> horizontal(sampleCount);
    std::vector<float> blurred(sampleCount);
    for (size_t i = 0; i < sampleCount; ++i)
        source[i] = hf.samples[i].normalizedHeight;

    // Separable Gaussian blur: O(width * height * radius) instead of the old
    // O(width * height * radius^2). Large radii are now practical for PNG maps.
    radius = std::clamp(radius, 1, 128);
    const float sigma = std::max(static_cast<float>(radius) * 0.45f, 0.5f);
    std::vector<float> weights(static_cast<size_t>(radius + 1));
    weights[0] = 1.0f;
    for (int i = 1; i <= radius; ++i)
        weights[static_cast<size_t>(i)] = std::exp(-(static_cast<float>(i * i)) / (2.0f * sigma * sigma));

    for (int pass = 0; pass < passes; ++pass)
    {
        for (uint32_t z = 0; z < height; ++z)
        {
            for (uint32_t x = 0; x < width; ++x)
            {
                float sum = source[static_cast<size_t>(z) * width + x] * weights[0];
                float total = weights[0];
                for (int dx = 1; dx <= radius; ++dx)
                {
                    const uint32_t xl = static_cast<uint32_t>(std::max(static_cast<int>(x) - dx, 0));
                    const uint32_t xr = static_cast<uint32_t>(std::min(static_cast<int>(x) + dx, static_cast<int>(width - 1)));
                    const float w = weights[static_cast<size_t>(dx)];
                    sum += (source[static_cast<size_t>(z) * width + xl] +
                            source[static_cast<size_t>(z) * width + xr]) * w;
                    total += 2.0f * w;
                }
                horizontal[static_cast<size_t>(z) * width + x] = sum / total;
            }
        }

        for (uint32_t z = 0; z < height; ++z)
        {
            for (uint32_t x = 0; x < width; ++x)
            {
                float sum = horizontal[static_cast<size_t>(z) * width + x] * weights[0];
                float total = weights[0];
                for (int dz = 1; dz <= radius; ++dz)
                {
                    const uint32_t zd = static_cast<uint32_t>(std::max(static_cast<int>(z) - dz, 0));
                    const uint32_t zu = static_cast<uint32_t>(std::min(static_cast<int>(z) + dz, static_cast<int>(height - 1)));
                    const float w = weights[static_cast<size_t>(dz)];
                    sum += (horizontal[static_cast<size_t>(zd) * width + x] +
                            horizontal[static_cast<size_t>(zu) * width + x]) * w;
                    total += 2.0f * w;
                }

                const size_t idx = static_cast<size_t>(z) * width + x;
                blurred[idx] = Lerp(source[idx], sum / total, blend);
            }
        }

        source.swap(blurred);
    }

    for (uint32_t z = 0; z < height; ++z)
    {
        for (uint32_t x = 0; x < width; ++x)
        {
            const size_t idx = static_cast<size_t>(z) * width + x;
            TerrainSample& sample = hf.At(x, z);
            sample.normalizedHeight = Clamp01(source[idx]);
            sample.height = sample.normalizedHeight * hf.settings.heightScale;
        }
    }

    UpdateHeightBounds(hf);
}

void SmoothHeightfield(TerrainHeightfield& hf, int passes, bool median)
{
    if (!hf.IsValid() || passes <= 0)
        return;

    const uint32_t width = hf.width;
    const uint32_t height = hf.height;
    std::vector<float> tmp(static_cast<size_t>(width) * height);
    std::vector<float> window;

    for (int pass = 0; pass < passes; ++pass)
    {
        // Grow the filter footprint as the user increases smoothing. A fixed
        // 3x3 blur barely affects high-resolution PNG heightmaps, so stronger
        // values now blend over a wider neighbourhood instead of requiring
        // dozens of visually identical passes.
        const int radius = median ? std::min(2 + pass / 4, 5)
                                  : std::min(1 + pass / 2, 8);
        if (median)
            window.resize(static_cast<size_t>((radius * 2 + 1) * (radius * 2 + 1)));

        for (uint32_t z = 0; z < height; ++z)
        {
            for (uint32_t x = 0; x < width; ++x)
            {
                int count = 0;
                float weightedSum = 0.0f;
                float weightTotal = 0.0f;
                for (int dz = -radius; dz <= radius; ++dz)
                {
                    for (int dx = -radius; dx <= radius; ++dx)
                    {
                        const int nx = static_cast<int>(x) + dx;
                        const int nz = static_cast<int>(z) + dz;
                        if (nx < 0 || nx >= static_cast<int>(width) || nz < 0 || nz >= static_cast<int>(height))
                            continue;

                        const float value = hf.At(static_cast<uint32_t>(nx),
                                                  static_cast<uint32_t>(nz)).normalizedHeight;
                        if (median)
                        {
                            window[static_cast<size_t>(count++)] = value;
                        }
                        else
                        {
                            const float dist2 = static_cast<float>(dx * dx + dz * dz);
                            const float sigma = std::max(static_cast<float>(radius) * 0.55f, 0.5f);
                            const float weight = std::exp(-dist2 / (2.0f * sigma * sigma));
                            weightedSum += value * weight;
                            weightTotal += weight;
                        }
                    }
                }

                const size_t idx = static_cast<size_t>(z) * width + x;
                if (median)
                {
                    std::sort(window.begin(), window.begin() + count);
                    tmp[idx] = window[static_cast<size_t>(count / 2)];
                }
                else
                {
                    tmp[idx] = weightedSum / std::max(weightTotal, 1e-6f);
                }
            }
        }

        for (uint32_t z = 0; z < height; ++z)
        {
            for (uint32_t x = 0; x < width; ++x)
            {
                TerrainSample& sample = hf.At(x, z);
                const float normalized = Clamp01(tmp[static_cast<size_t>(z) * width + x]);
                sample.normalizedHeight = normalized;
                sample.height = normalized * hf.settings.heightScale;
            }
        }
    }

    UpdateHeightBounds(hf);
}

void ApplyHydraulicErosion(TerrainHeightfield& hf, const TerrainGenerationSettings& settings)
{
    if (!hf.IsValid() || !settings.erosionEnabled || settings.erosionIterations <= 0)
        return;

    const uint32_t width = hf.width;
    const uint32_t height = hf.height;
    const size_t sampleCount = static_cast<size_t>(width) * height;
    std::vector<float> heights(sampleCount);
    std::vector<float> erosionAmount(sampleCount, 0.0f);
    std::vector<float> depositionAmount(sampleCount, 0.0f);

    for (size_t idx = 0; idx < sampleCount; ++idx)
        heights[idx] = hf.samples[idx].height;

    std::mt19937 rng(settings.seed);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    constexpr int kMaxDropletSteps = 30;
    constexpr float kMinSedimentCapacity = 0.01f;

    auto distributeDelta = [&](float fx, float fz, float delta,
                               std::vector<float>* amountTracker)
    {
        fx = std::clamp(fx, 0.0f, static_cast<float>(width - 1));
        fz = std::clamp(fz, 0.0f, static_cast<float>(height - 1));

        const uint32_t x0 = static_cast<uint32_t>(std::floor(fx));
        const uint32_t z0 = static_cast<uint32_t>(std::floor(fz));
        const uint32_t x1 = std::min(x0 + 1, width - 1);
        const uint32_t z1 = std::min(z0 + 1, height - 1);
        const float sx = fx - static_cast<float>(x0);
        const float sz = fz - static_cast<float>(z0);

        const float w00 = (1.0f - sx) * (1.0f - sz);
        const float w10 = sx * (1.0f - sz);
        const float w01 = (1.0f - sx) * sz;
        const float w11 = sx * sz;

        const size_t i00 = static_cast<size_t>(z0) * width + x0;
        const size_t i10 = static_cast<size_t>(z0) * width + x1;
        const size_t i01 = static_cast<size_t>(z1) * width + x0;
        const size_t i11 = static_cast<size_t>(z1) * width + x1;

        heights[i00] += delta * w00;
        heights[i10] += delta * w10;
        heights[i01] += delta * w01;
        heights[i11] += delta * w11;

        if (amountTracker)
        {
            (*amountTracker)[i00] += std::abs(delta) * w00;
            (*amountTracker)[i10] += std::abs(delta) * w10;
            (*amountTracker)[i01] += std::abs(delta) * w01;
            (*amountTracker)[i11] += std::abs(delta) * w11;
        }
    };

    for (int iteration = 0; iteration < settings.erosionIterations; ++iteration)
    {
        float posX = dist(rng) * static_cast<float>(width - 1);
        float posZ = dist(rng) * static_cast<float>(height - 1);
        float dirX = 0.0f;
        float dirZ = 0.0f;
        float speed = 1.0f;
        float water = 1.0f;
        float sediment = 0.0f;

        for (int step = 0; step < kMaxDropletSteps; ++step)
        {
            const float currentHeight = SampleHeightfield(heights, width, height, posX, posZ);
            const glm::vec2 gradient = SampleHeightfieldGradient(heights, width, height, posX, posZ);

            dirX = dirX * settings.erosionInertia - gradient.x * (1.0f - settings.erosionInertia);
            dirZ = dirZ * settings.erosionInertia - gradient.y * (1.0f - settings.erosionInertia);

            float dirLen = std::sqrt(dirX * dirX + dirZ * dirZ);
            if (dirLen < 1e-5f)
            {
                dirX = dist(rng) - 0.5f;
                dirZ = dist(rng) - 0.5f;
                dirLen = std::sqrt(dirX * dirX + dirZ * dirZ);
                if (dirLen < 1e-5f)
                    break;
            }
            dirX /= dirLen;
            dirZ /= dirLen;

            const float nextX = posX + dirX;
            const float nextZ = posZ + dirZ;
            if (nextX < 0.0f || nextX > static_cast<float>(width - 1) || nextZ < 0.0f || nextZ > static_cast<float>(height - 1))
                break;

            const float nextHeight = SampleHeightfield(heights, width, height, nextX, nextZ);
            const float deltaHeight = nextHeight - currentHeight;
            const float descent = std::max(-deltaHeight, 0.0f);
            const float slopeGate = Smoothstep(settings.erosionMinSlope,
                                               settings.erosionMinSlope * 4.0f + 1e-4f,
                                               descent);
            const float capacity = std::max(descent * speed * water * settings.erosionCapacity,
                                            kMinSedimentCapacity) * slopeGate;

            if (deltaHeight > 0.0f || sediment > capacity)
            {
                const float deposit = deltaHeight > 0.0f
                    ? std::min(sediment, deltaHeight)
                    : (sediment - capacity) * settings.erosionDeposition;
                if (deposit > 0.0f)
                {
                    distributeDelta(posX, posZ, deposit, &depositionAmount);
                    sediment -= deposit;
                }
            }
            else
            {
                const float erode = std::min((capacity - sediment) * settings.erosionErosion,
                                             currentHeight) * slopeGate;
                if (erode > 0.0f)
                {
                    distributeDelta(posX, posZ, -erode, &erosionAmount);
                    sediment += erode;
                }
            }

            speed = std::sqrt(std::max(speed * speed + deltaHeight * 4.0f, 0.0f));
            water *= std::max(1.0f - settings.erosionEvaporation, 0.0f);
            if (water <= 0.0f)
                break;

            posX = nextX;
            posZ = nextZ;
        }
    }

    for (uint32_t z = 0; z < height; ++z)
    {
        for (uint32_t x = 0; x < width; ++x)
        {
            const size_t idx = static_cast<size_t>(z) * width + x;
            TerrainSample& sample = hf.At(x, z);
            // Clamp raw height: deposition can push heights above heightScale;
            // erosion in theory can't go negative (guarded above), but clamp anyway.
            sample.height = std::clamp(heights[idx], 0.0f, hf.settings.heightScale);
            sample.normalizedHeight = sample.height / hf.settings.heightScale;
            sample.erosionAmount = erosionAmount[idx];
            sample.depositionAmount = depositionAmount[idx];
        }
    }

    UpdateHeightBounds(hf);
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
            const float mountains = Fbm(wwx * settings.mountainScale, wwz * settings.mountainScale,
                                       settings.seed + 1511u, settings.mountainOctaves,
                                       settings.mountainPersistence, settings.mountainLacunarity);
            const float mountainRidges = MountainRidgeVariation(wwx, wwz, settings);
            const float detail = ValueNoise(wwx * settings.detailScale, wwz * settings.detailScale, settings.seed + 307u) - 0.5f;
            const float broadHillShape = MacroShapeVariation(wwx, wwz, settings.broadHillScale, settings.seed + 811u);
            const float valleyShape = MacroShapeVariation(wwx, wwz, settings.valleyScale, settings.seed + 907u);

            float h = 0.08f;
            h += (macro - 0.5f) * settings.macroAmplitude * 0.55f;
            h += broadHillShape * Clamp01(settings.broadHillStrength) * regionMask.broadHills;
            h -= valleyShape * Clamp01(settings.valleyStrength) * regionMask.valleys;
            h += mountains * settings.mountainAmplitude * mountainMask;
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

    // Separable Gaussian first: removes the broad high-frequency spikes that the
    // weighted-average pass below can't reach in one or two passes.
    if (settings.proceduralBlurPasses > 0)
        GaussianSmoothHeightfield(hf, settings.proceduralBlurPasses,
                                  std::max(1, settings.proceduralBlurRadius),
                                  settings.proceduralBlurStrength);

    // Weighted-average pass: tightens up residual fine-scale noise.
    SmoothHeightfield(hf, settings.heightSmoothingPasses, settings.heightSmoothingMedian);

    if (settings.erosionEnabled)
    {
        ApplyHydraulicErosion(hf, settings);
        // Gaussian pass first to soften sharp erosion walls, then average smooth.
        if (settings.postErosionBlurPasses > 0)
            GaussianSmoothHeightfield(hf, settings.postErosionBlurPasses,
                                      std::max(1, settings.postErosionBlurRadius),
                                      settings.postErosionBlurStrength);
        SmoothHeightfield(hf, settings.postErosionSmoothingPasses, settings.heightSmoothingMedian);
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
    hf.settings        = settings;
    hf.width           = static_cast<uint32_t>(w);
    hf.height          = static_cast<uint32_t>(h);
    // Preserve the image's aspect ratio in world space so non-square maps
    // don't get squashed.  worldWidth stays as configured; worldHeight scales.
    hf.settings.worldHeight = settings.worldWidth * (static_cast<float>(h) / static_cast<float>(w));
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

    // Remap to [0, 1] based on the actual pixel range so the full heightScale
    // is used regardless of what grey levels the image occupies.
    {
        float rawMin = std::numeric_limits<float>::max();
        float rawMax = std::numeric_limits<float>::lowest();
        for (const TerrainSample& s : hf.samples)
        {
            rawMin = std::min(rawMin, s.normalizedHeight);
            rawMax = std::max(rawMax, s.normalizedHeight);
        }
        const float range = rawMax - rawMin;
        if (range > 1e-6f)
        {
            const float invRange = 1.0f / range;
            for (TerrainSample& s : hf.samples)
            {
                s.normalizedHeight = (s.normalizedHeight - rawMin) * invRange;
                s.height           = s.normalizedHeight * settings.heightScale;
            }
        }
        spdlog::info("[TerrainGenerator] Heightmap pixel range remapped: [{:.4f}, {:.4f}] → [0, 1]",
                     rawMin, rawMax);
    }

    // Update heightfield bounds after remapping.
    hf.minHeight = 0.0f;
    hf.maxHeight = settings.heightScale;

    // Smooth imported maps before erosion. PNG heightmaps often contain hard,
    // pointy high-frequency data. SmoothHeightfield now grows its filter radius
    // per pass, so each UI step should visibly soften silhouettes.
    const int heightmapSmoothPasses = std::max(0, settings.heightmapSmoothPasses);
    if (heightmapSmoothPasses > 0)
        GaussianSmoothHeightfield(hf, heightmapSmoothPasses,
                                  std::max(1, settings.heightmapBlurRadius),
                                  settings.heightmapBlurStrength);

    SmoothHeightfield(hf, settings.heightSmoothingPasses, settings.heightSmoothingMedian);

    if (settings.erosionEnabled)
    {
        ApplyHydraulicErosion(hf, settings);
        if (settings.postErosionBlurPasses > 0)
            GaussianSmoothHeightfield(hf, settings.postErosionBlurPasses,
                                      std::max(1, settings.postErosionBlurRadius),
                                      settings.postErosionBlurStrength);
        SmoothHeightfield(hf, settings.postErosionSmoothingPasses, settings.heightSmoothingMedian);
    }

    RebuildDerivedData(hf, classificationSettings);
    return hf;
}
}
