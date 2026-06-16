#include "terrain/TerrainGenerator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <random>
#include <vector>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>
#include <stb_image.h>

namespace
{
constexpr int kRegionMaskOctaves = 2;
constexpr int kMacroShapeOctaves = 2;

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

// 8 gradient directions: 4 axis-aligned + 4 diagonal. Replacing value noise
// (hash-interpolated) with gradient (Perlin) noise removes the blobby
// axis-aligned banding that appears in value noise at visible frequencies.
static constexpr float kGrads8[8][2] = {
    { 1.0f,  0.0f}, {-1.0f,  0.0f},
    { 0.0f,  1.0f}, { 0.0f, -1.0f},
    { 0.7071f,  0.7071f}, {-0.7071f,  0.7071f},
    { 0.7071f, -0.7071f}, {-0.7071f, -0.7071f}
};
// Maximum absolute output of the gradient dot products before interpolation is
// ~0.7071 (diagonal gradients at mid-cell). Dividing by this remaps to [-1, 1]
// then we shift to [0, 1] so the output matches the old ValueNoise range and
// all Fbm / Smoothstep calls above are unchanged.
constexpr float kGradNorm = 1.0f / 0.7071f * 0.5f;  // ≈ 0.7071

float GradientNoise(float x, float y, uint32_t seed)
{
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const float tx = x - static_cast<float>(x0);
    const float ty = y - static_cast<float>(y0);
    const float sx = tx * tx * (3.0f - 2.0f * tx);
    const float sy = ty * ty * (3.0f - 2.0f * ty);

    auto Dot = [&](int ix, int iy, float fx, float fy) -> float {
        const uint32_t gi = Hash(static_cast<uint32_t>(ix),
                                  static_cast<uint32_t>(iy), seed) & 7u;
        return kGrads8[gi][0] * fx + kGrads8[gi][1] * fy;
    };

    const float n00 = Dot(x0,     y0,     tx,        ty       );
    const float n10 = Dot(x0 + 1, y0,     tx - 1.0f, ty       );
    const float n01 = Dot(x0,     y0 + 1, tx,        ty - 1.0f);
    const float n11 = Dot(x0 + 1, y0 + 1, tx - 1.0f, ty - 1.0f);

    const float raw = Lerp(Lerp(n00, n10, sx), Lerp(n01, n11, sx), sy);
    return Clamp01(raw * kGradNorm + 0.5f);
}

float Fbm(float x, float y, uint32_t seed, int octaves, float persistence, float lacunarity)
{
    float value = 0.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float norm = 0.0f;
    for (int i = 0; i < std::max(1, octaves); ++i)
    {
        value += GradientNoise(x * frequency, y * frequency, seed + static_cast<uint32_t>(i) * 1013u) * amplitude;
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

    // Flat index range used by both separable passes.
    std::vector<size_t> rowIndices(height), colIndices(width);
    std::iota(rowIndices.begin(), rowIndices.end(), size_t{0});
    std::iota(colIndices.begin(), colIndices.end(), size_t{0});

    for (int pass = 0; pass < passes; ++pass)
    {
        // Horizontal pass — each row is independent.
        std::for_each(rowIndices.begin(), rowIndices.end(),
            [&](size_t z)
            {
                for (uint32_t x = 0; x < width; ++x)
                {
                    float sum   = source[z * width + x] * weights[0];
                    float total = weights[0];
                    for (int dx = 1; dx <= radius; ++dx)
                    {
                        const uint32_t xl = static_cast<uint32_t>(std::max(static_cast<int>(x) - dx, 0));
                        const uint32_t xr = static_cast<uint32_t>(std::min(static_cast<int>(x) + dx, static_cast<int>(width - 1)));
                        const float w = weights[static_cast<size_t>(dx)];
                        sum   += (source[z * width + xl] + source[z * width + xr]) * w;
                        total += 2.0f * w;
                    }
                    horizontal[z * width + x] = sum / total;
                }
            });

        // Vertical pass — each column is independent.
        std::for_each(colIndices.begin(), colIndices.end(),
            [&](size_t x)
            {
                for (uint32_t z = 0; z < height; ++z)
                {
                    float sum   = horizontal[z * width + x] * weights[0];
                    float total = weights[0];
                    for (int dz = 1; dz <= radius; ++dz)
                    {
                        const uint32_t zd = static_cast<uint32_t>(std::max(static_cast<int>(z) - dz, 0));
                        const uint32_t zu = static_cast<uint32_t>(std::min(static_cast<int>(z) + dz, static_cast<int>(height - 1)));
                        const float w = weights[static_cast<size_t>(dz)];
                        sum   += (horizontal[zd * width + x] + horizontal[zu * width + x]) * w;
                        total += 2.0f * w;
                    }
                    const size_t idx = z * width + x;
                    blurred[idx] = Lerp(source[idx], sum / total, blend);
                }
            });

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

    const uint32_t width  = hf.width;
    const uint32_t height = hf.height;
    const size_t   total  = static_cast<size_t>(width) * height;

    std::vector<float> tmp(total);

    // Flat index range for parallel_for
    std::vector<size_t> indices(total);
    std::iota(indices.begin(), indices.end(), size_t{0});

    for (int pass = 0; pass < passes; ++pass)
    {
        // Grow the kernel footprint with each pass so later passes cover coarser
        // features without requiring an unreasonable number of iterations.
        const int radius = median ? std::min(2 + pass / 4, 5)
                                  : std::min(1 + pass / 2, 8);

        if (median)
        {
            // Median: each pixel sorts its neighbourhood — thread-local window.
            const int diam = 2 * radius + 1;
            std::for_each(indices.begin(), indices.end(),
                [&](size_t idx)
                {
                    const uint32_t x = static_cast<uint32_t>(idx % width);
                    const uint32_t z = static_cast<uint32_t>(idx / width);

                    std::vector<float> window;
                    window.reserve(static_cast<size_t>(diam * diam));
                    for (int dz = -radius; dz <= radius; ++dz)
                    for (int dx = -radius; dx <= radius; ++dx)
                    {
                        const int nx = std::clamp(static_cast<int>(x) + dx, 0, static_cast<int>(width)  - 1);
                        const int nz = std::clamp(static_cast<int>(z) + dz, 0, static_cast<int>(height) - 1);
                        window.push_back(hf.At(static_cast<uint32_t>(nx),
                                               static_cast<uint32_t>(nz)).normalizedHeight);
                    }
                    std::sort(window.begin(), window.end());
                    tmp[idx] = window[window.size() / 2];
                });
        }
        else
        {
            // Gaussian: precompute the 2D weight table once per pass — avoids
            // exp() inside the hot per-pixel loop.
            const float sigma   = std::max(static_cast<float>(radius) * 0.55f, 0.5f);
            const float inv2s2  = 1.0f / (2.0f * sigma * sigma);
            const int   diam    = 2 * radius + 1;
            std::vector<float> wTable(static_cast<size_t>(diam * diam));
            float wNorm = 0.0f;
            for (int dz = -radius; dz <= radius; ++dz)
            for (int dx = -radius; dx <= radius; ++dx)
            {
                const float w = std::exp(-static_cast<float>(dx * dx + dz * dz) * inv2s2);
                wTable[static_cast<size_t>((dz + radius) * diam + (dx + radius))] = w;
                wNorm += w;
            }
            const float invNorm = 1.0f / wNorm;
            for (float& w : wTable) w *= invNorm;

            std::for_each(indices.begin(), indices.end(),
                [&](size_t idx)
                {
                    const uint32_t x = static_cast<uint32_t>(idx % width);
                    const uint32_t z = static_cast<uint32_t>(idx / width);
                    float sum = 0.0f;
                    for (int dz = -radius; dz <= radius; ++dz)
                    for (int dx = -radius; dx <= radius; ++dx)
                    {
                        const int nx = std::clamp(static_cast<int>(x) + dx, 0, static_cast<int>(width)  - 1);
                        const int nz = std::clamp(static_cast<int>(z) + dz, 0, static_cast<int>(height) - 1);
                        sum += hf.At(static_cast<uint32_t>(nx),
                                     static_cast<uint32_t>(nz)).normalizedHeight
                             * wTable[static_cast<size_t>((dz + radius) * diam + (dx + radius))];
                    }
                    tmp[idx] = sum;
                });
        }

        // Write results back — also parallel since each element is independent.
        const float hs = hf.settings.heightScale;
        std::for_each(indices.begin(), indices.end(),
            [&](size_t idx)
            {
                const float n = Clamp01(tmp[idx]);
                hf.samples[idx].normalizedHeight = n;
                hf.samples[idx].height           = n * hs;
            });
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

// Thermal (talus) erosion: transfers material from cells steeper than the angle
// of repose to their downslope neighbour. Runs in full-grid passes; each cell's
// delta is accumulated in a separate buffer and applied atomically per pass so
// the order of traversal doesn't bias the result.
void ApplyThermalErosion(TerrainHeightfield& hf, const TerrainGenerationSettings& settings)
{
    if (!hf.IsValid() || !settings.thermalErosionEnabled || settings.thermalErosionIterations <= 0)
        return;

    const uint32_t W   = hf.width;
    const uint32_t H   = hf.height;
    const size_t   N   = static_cast<size_t>(W) * H;
    const float    hs  = hf.settings.heightScale;
    const float    dx  = hf.settings.worldWidth  / static_cast<float>(W - 1);
    const float    dz  = hf.settings.worldHeight / static_cast<float>(H - 1);
    // Height difference corresponding to the repose angle over one cell diagonal.
    const float    threshold = std::tan(glm::radians(settings.thermalErosionAngle))
                               * std::min(dx, dz);
    const float    rate = std::clamp(settings.thermalErosionStrength, 0.0f, 1.0f) * 0.5f;

    std::vector<float> heights(N);
    for (size_t i = 0; i < N; ++i) heights[i] = hf.samples[i].height;

    std::vector<float> deltas(N);

    // 4-connected neighbours: right, left, forward, back.
    static constexpr int kDX[4] = { 1, -1, 0,  0};
    static constexpr int kDZ[4] = { 0,  0, 1, -1};

    for (int iter = 0; iter < settings.thermalErosionIterations; ++iter)
    {
        std::fill(deltas.begin(), deltas.end(), 0.0f);

        for (uint32_t z = 0; z < H; ++z)
        for (uint32_t x = 0; x < W; ++x)
        {
            const float h = heights[z * W + x];
            for (int n = 0; n < 4; ++n)
            {
                const int nx = static_cast<int>(x) + kDX[n];
                const int nz = static_cast<int>(z) + kDZ[n];
                if (nx < 0 || nx >= static_cast<int>(W) || nz < 0 || nz >= static_cast<int>(H))
                    continue;
                const float diff = h - heights[static_cast<size_t>(nz) * W + nx];
                if (diff > threshold)
                {
                    const float transfer = (diff - threshold) * rate;
                    deltas[z * W + x]                                   -= transfer;
                    deltas[static_cast<size_t>(nz) * W + nx] += transfer;
                }
            }
        }

        for (size_t i = 0; i < N; ++i)
            heights[i] = std::clamp(heights[i] + deltas[i], 0.0f, hs);
    }

    for (size_t i = 0; i < N; ++i)
    {
        hf.samples[i].height           = heights[i];
        hf.samples[i].normalizedHeight = heights[i] / hs;
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
        const float n = GradientNoise(x * frequency, y * frequency, seed + static_cast<uint32_t>(i) * 9176u);
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

// Evaluate the full multi-layer height stack for one world-space position.
// Extracted so the parallel generation loop stays readable and so it can be
// unit-tested or reused independently.
float EvaluateHeightStack(float wx, float wz,
                          const TerrainRegionMaskSample& regionMask,
                          const TerrainGenerationSettings& s)
{
    // Domain warp: offsets noise coordinates to break blobby regularity.
    // Region masks and volcano distance are NOT warped (would corrupt geography).
    float wwx = wx, wwz = wz;
    if (s.domainWarpEnabled)
    {
        wwx += (GradientNoise(wx * s.domainWarpScale, wz * s.domainWarpScale, s.seed + 3001u) - 0.5f)
               * s.domainWarpStrength;
        wwz += (GradientNoise(wx * s.domainWarpScale, wz * s.domainWarpScale, s.seed + 3002u) - 0.5f)
               * s.domainWarpStrength;
    }

    const float mountainMask   = regionMask.mountains;
    const float macro          = Fbm(wwx * s.macroScale,    wwz * s.macroScale,    s.seed + 11u,   3, 0.55f, 2.0f);
    const float hills          = Fbm(wwx * s.hillScale,     wwz * s.hillScale,     s.seed + 101u,
                                     s.hillOctaves, s.hillPersistence, s.hillLacunarity);
    const float mountains      = Fbm(wwx * s.mountainScale, wwz * s.mountainScale, s.seed + 1511u,
                                     s.mountainOctaves, s.mountainPersistence, s.mountainLacunarity);
    const float mountainRidges = MountainRidgeVariation(wwx, wwz, s);
    const float detail         = GradientNoise(wwx * s.detailScale, wwz * s.detailScale, s.seed + 307u) - 0.5f;
    const float broadHillShape = MacroShapeVariation(wwx, wwz, s.broadHillScale, s.seed + 811u);
    const float valleyShape    = MacroShapeVariation(wwx, wwz, s.valleyScale,    s.seed + 907u);

    float h = 0.08f;
    h += (macro - 0.5f) * s.macroAmplitude * 0.55f;
    h += broadHillShape  * Clamp01(s.broadHillStrength)   * regionMask.broadHills;
    h -= valleyShape     * Clamp01(s.valleyStrength)       * regionMask.valleys;
    h += mountains       * s.mountainAmplitude             * mountainMask;
    h += hills           * s.hillAmplitude                 * (1.0f - mountainMask * 0.35f);
    h += mountainRidges  * Clamp01(s.mountainRidgeStrength)* mountainMask;
    h += detail          * s.detailAmplitude;

    // Volcano radial pedestal — sampled at original (wx,wz) so the cone stays
    // centred regardless of domain warp.
    if (s.volcanoEnabled)
    {
        const float r       = glm::length(glm::vec2(wx, wz)) / (s.worldWidth * 0.5f);
        const float rimR    = s.volcanoRimRadius;
        const float cone    = std::max(0.0f, 1.0f - std::abs(r - rimR) / rimR) * s.volcanoConeHeight;
        const float caldera = Smoothstep(rimR * s.volcanoCalderaOuterRatio,
                                         rimR * s.volcanoCalderaInnerRatio,
                                         r) * s.volcanoCalderaDepth;
        h += cone - caldera;
    }

    h = Clamp01(h);
    h = Clamp01(ApplyPlateauShaping(h, wwx, wwz, regionMask, s));
    return h;
}

namespace TerrainGenerator
{
TerrainHeightfield GenerateHeightfield(const TerrainGenerationSettings& settings,
                                       const TerrainClassificationSettings& classificationSettings)
{
    TerrainHeightfield hf;
    hf.width    = std::max(2u, settings.gridWidth);
    hf.height   = std::max(2u, settings.gridHeight);
    hf.settings = settings;
    hf.samples.resize(static_cast<size_t>(hf.width) * hf.height);
    hf.minHeight =  std::numeric_limits<float>::max();
    hf.maxHeight = -std::numeric_limits<float>::max();

    const TerrainRegionMasks regionMasks = BuildTerrainRegionMasks(settings, hf.width, hf.height);

    // Flat index range keeps the 2-D grid traversal simple and cache-friendly.
    const size_t total = static_cast<size_t>(hf.width) * hf.height;
    std::vector<size_t> indices(total);
    std::iota(indices.begin(), indices.end(), size_t{0});

    std::for_each(indices.begin(), indices.end(),
        [&](size_t idx)
        {
            const uint32_t x = static_cast<uint32_t>(idx % hf.width);
            const uint32_t z = static_cast<uint32_t>(idx / hf.width);

            const float wx = (static_cast<float>(x) / static_cast<float>(hf.width  - 1) - 0.5f) * settings.worldWidth;
            const float wz = (static_cast<float>(z) / static_cast<float>(hf.height - 1) - 0.5f) * settings.worldHeight;

            const float h = EvaluateHeightStack(wx, wz, regionMasks.At(x, z), settings);

            TerrainSample& sample   = hf.samples[idx];
            sample.normalizedHeight = h;
            sample.height           = h * settings.heightScale;
            sample.mountainMask     = regionMasks.At(x, z).mountains;
        });

    UpdateHeightBounds(hf);

    // ── Smooth ────────────────────────────────────────────────────────────────
    if (settings.proceduralBlurPasses > 0)
        GaussianSmoothHeightfield(hf, settings.proceduralBlurPasses,
                                  std::max(1, settings.proceduralBlurRadius),
                                  settings.proceduralBlurStrength);
    SmoothHeightfield(hf, settings.heightSmoothingPasses, settings.heightSmoothingMedian);

    // ── Hydraulic erosion ─────────────────────────────────────────────────────
    if (settings.erosionEnabled)
    {
        ApplyHydraulicErosion(hf, settings);
        if (settings.postErosionBlurPasses > 0)
            GaussianSmoothHeightfield(hf, settings.postErosionBlurPasses,
                                      std::max(1, settings.postErosionBlurRadius),
                                      settings.postErosionBlurStrength);
        SmoothHeightfield(hf, settings.postErosionSmoothingPasses, settings.heightSmoothingMedian);
    }

    // ── Thermal erosion ───────────────────────────────────────────────────────
    if (settings.thermalErosionEnabled)
        ApplyThermalErosion(hf, settings);

    RebuildDerivedData(hf, classificationSettings);
    return hf;
}

void RebuildDerivedData(TerrainHeightfield& hf, const TerrainClassificationSettings& c)
{
    if (!hf.IsValid()) return;

    // Precompute world-space X/Z positions for each grid column/row once to
    // avoid repeated floating-point division inside the (potentially parallel) loop.
    std::vector<float> worldX(hf.width), worldZ(hf.height);
    for (uint32_t i = 0; i < hf.width;  ++i)
        worldX[i] = (static_cast<float>(i) / static_cast<float>(hf.width  - 1) - 0.5f) * hf.settings.worldWidth;
    for (uint32_t i = 0; i < hf.height; ++i)
        worldZ[i] = (static_cast<float>(i) / static_cast<float>(hf.height - 1) - 0.5f) * hf.settings.worldHeight;

    // Helper: world-space position at clamped grid coords (reads only .height).
    auto P = [&](int px, int pz) -> glm::vec3
    {
        const uint32_t cx = static_cast<uint32_t>(std::clamp(px, 0, static_cast<int>(hf.width)  - 1));
        const uint32_t cz = static_cast<uint32_t>(std::clamp(pz, 0, static_cast<int>(hf.height) - 1));
        return { worldX[cx], hf.At(cx, cz).height, worldZ[cz] };
    };

    const size_t total = static_cast<size_t>(hf.width) * hf.height;
    std::vector<size_t> indices(total);
    std::iota(indices.begin(), indices.end(), size_t{0});

    std::for_each(indices.begin(), indices.end(),
        [&](size_t idx)
        {
        const uint32_t x = static_cast<uint32_t>(idx % hf.width);
        const uint32_t z = static_cast<uint32_t>(idx / hf.width);

        // Face-normal averaging: accumulate area-weighted cross products from
        // the four surrounding quads. Matches the rendered mesh exactly (the
        // mesh builder uses the same CCW triangulation order). Central differences
        // only approximate this and produce wrong normals at grid edges.
        const glm::vec3 center = P(static_cast<int>(x), static_cast<int>(z));
        glm::vec3 n(0.0f);
        const int ix = static_cast<int>(x), iz = static_cast<int>(z);
        if (x + 1 < hf.width  && z + 1 < hf.height)
            n += glm::cross(P(ix, iz+1) - center, P(ix+1, iz) - center);
        if (x > 0             && z + 1 < hf.height)
            n += glm::cross(P(ix-1, iz) - center, P(ix,   iz+1) - center);
        if (x > 0             && z > 0)
            n += glm::cross(P(ix, iz-1) - center, P(ix-1, iz) - center);
        if (x + 1 < hf.width  && z > 0)
            n += glm::cross(P(ix+1, iz) - center, P(ix,   iz-1) - center);

        TerrainSample& s = hf.At(x, z);
        s.normal = glm::length(n) > 1e-8f ? glm::normalize(n) : glm::vec3(0.0f, 1.0f, 0.0f);
        s.slope  = Clamp01(1.0f - s.normal.y);

        const float h01     = Clamp01(s.normalizedHeight);
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
        });
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

    // Decode raw pixels into a flat float array so we can bilinearly resample
    // to the requested output grid resolution (gridWidth × gridHeight).
    const size_t imgPixels = static_cast<size_t>(w) * h;
    std::vector<float> rawHeights(imgPixels);
    for (int z = 0; z < h; ++z)
    {
        const int srcZ = settings.heightmapFlipY ? (h - 1 - z) : z;
        for (int x = 0; x < w; ++x)
        {
            const size_t srcIdx = static_cast<size_t>(srcZ) * w + x;
            rawHeights[static_cast<size_t>(z) * w + x] = is16
                ? static_cast<float>(px16[srcIdx]) / 65535.0f
                : static_cast<float>(px8[srcIdx])  / 255.0f;
        }
    }

    if (is16) stbi_image_free(px16);
    else      stbi_image_free(px8);

    const uint32_t gridW = std::max(2u, settings.gridWidth);
    const uint32_t gridH = std::max(2u, settings.gridHeight);

    TerrainHeightfield hf;
    hf.settings        = settings;
    hf.width           = gridW;
    hf.height          = gridH;
    hf.settings.gridWidth  = gridW;
    hf.settings.gridHeight = gridH;
    // Preserve the PNG's aspect ratio in world space regardless of output grid size.
    hf.settings.worldHeight = settings.worldWidth * (static_cast<float>(h) / static_cast<float>(w));
    hf.samples.resize(static_cast<size_t>(gridW) * gridH);
    hf.minHeight =  std::numeric_limits<float>::max();
    hf.maxHeight = -std::numeric_limits<float>::max();

    // Bilinearly resample from the full-res pixel grid to the output mesh grid.
    // Halving gridW/H quarters the triangle count; quality loss is minimal for
    // smooth heightmaps because the blur passes already killed sub-grid detail.
    for (uint32_t gz = 0; gz < gridH; ++gz)
    {
        const float fz = static_cast<float>(gz) / static_cast<float>(gridH - 1)
                         * static_cast<float>(h - 1);
        for (uint32_t gx = 0; gx < gridW; ++gx)
        {
            const float fx = static_cast<float>(gx) / static_cast<float>(gridW - 1)
                             * static_cast<float>(w - 1);
            const float norm = SampleHeightfield(rawHeights,
                                                  static_cast<uint32_t>(w),
                                                  static_cast<uint32_t>(h),
                                                  fx, fz);
            TerrainSample& s   = hf.At(gx, gz);
            s.normalizedHeight = norm;
            s.height           = norm * settings.heightScale;
        }
    }

    // Remap raw pixel range [min, max] → [0, 1] so the full heightScale is
    // always used regardless of what grey levels the image occupies.
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

    // Gamma correction applied AFTER remapping so it always operates on [0, 1]
    // regardless of the image's native grey range. Applying it before remap
    // baked a different effective curve depending on raw pixel spread.
    if (settings.heightmapGamma != 1.0f)
    {
        for (TerrainSample& s : hf.samples)
        {
            s.normalizedHeight = std::pow(s.normalizedHeight, settings.heightmapGamma);
            s.height           = s.normalizedHeight * settings.heightScale;
        }
    }

    hf.minHeight = 0.0f;
    hf.maxHeight = settings.heightScale;
    UpdateHeightBounds(hf);

    // ── Smooth ────────────────────────────────────────────────────────────────
    if (settings.heightmapSmoothPasses > 0)
        GaussianSmoothHeightfield(hf, settings.heightmapSmoothPasses,
                                  std::max(1, settings.heightmapBlurRadius),
                                  settings.heightmapBlurStrength);
    SmoothHeightfield(hf, settings.heightSmoothingPasses, settings.heightSmoothingMedian);

    // ── Hydraulic erosion ─────────────────────────────────────────────────────
    if (settings.erosionEnabled)
    {
        ApplyHydraulicErosion(hf, settings);
        if (settings.postErosionBlurPasses > 0)
            GaussianSmoothHeightfield(hf, settings.postErosionBlurPasses,
                                      std::max(1, settings.postErosionBlurRadius),
                                      settings.postErosionBlurStrength);
        SmoothHeightfield(hf, settings.postErosionSmoothingPasses, settings.heightSmoothingMedian);
    }

    // ── Thermal erosion ───────────────────────────────────────────────────────
    if (settings.thermalErosionEnabled)
        ApplyThermalErosion(hf, settings);

    RebuildDerivedData(hf, classificationSettings);
    return hf;
}
}
