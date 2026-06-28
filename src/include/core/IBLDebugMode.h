#pragma once

#include <array>

enum class IBLDebugMode : int {
    OriginalEnvironment = 0,
    IrradianceCubemap = 1,
    PrefilteredCubemap = 2,
    PrefilteredMipLevel = 3,
    BrdfLut = 4,
    DiffuseIBLOnly = 5,
    SpecularIBLOnly = 6,
    FullIBLNoDirectLight = 7,
    FullLightingWithDirectAndIBL = 8,
};

inline constexpr IBLDebugMode kDefaultIBLDebugMode =
    IBLDebugMode::FullLightingWithDirectAndIBL;

inline constexpr std::array<const char*, 9> kIBLDebugModeLabels = {
    "Original Environment",
    "Irradiance Cubemap",
    "Prefiltered Cubemap",
    "Prefiltered Mip Level",
    "BRDF LUT",
    "Diffuse IBL Only",
    "Specular IBL Only",
    "Full IBL, No Direct Light",
    "Full Lighting + Direct + IBL",
};

inline constexpr int ToUniformValue(IBLDebugMode mode)
{
    return static_cast<int>(mode);
}
