#pragma once

#include "scene/LightEnvironment.h"
#include "scene/GpuLightData.h"
#include <cstdint>

/// Maximum light counts enforced at the GPU boundary.
/// Must stay in sync with kMaxPointLights / kMaxSpotLights in LightEnvironment.h
/// and with the array dimensions in the GLSL uniform block (binding = 1).
static constexpr int kGpuMaxPointLights = 16;
static constexpr int kGpuMaxSpotLights  = 8;

static_assert(kGpuMaxPointLights == kMaxPointLights,
    "GPU and CPU point light limits must match");
static_assert(kGpuMaxSpotLights == kMaxSpotLights,
    "GPU and CPU spot light limits must match");

/// The full contents of the per-frame light UBO (binding = 1).
///
/// GLSL declaration (add to every lit shader):
///
///   layout(std140, binding = 1) uniform LightBlock {
///       vec3  u_AmbientColor;        // offset   0
///       float u_AmbientIntensity;    // offset  12
///
///       int   u_NumPointLights;      // offset  16
///       int   u_NumSpotLights;       // offset  20
///       int   u_HasDirectional;      // offset  24
///       float _pad0;                 // offset  28
///
///       // DirectionalLight         // offset  32  (48 bytes)
///       vec3  u_DirDirection;        // offset  32
///       float u_DirIntensity;        // offset  44
///       vec3  u_DirColor;            // offset  48
///       uint  u_DirEnabled;          // offset  60
///       uint  u_DirCastShadow;       // offset  64
///       float u_DirDepthBias;        // offset  68
///       float u_DirNormalBias;       // offset  72
///       float _pad1;                 // offset  76
///
///       // PointLight[16]            // offset  80  (16 * 48 = 768 bytes)
///       // SpotLight[8]             // offset 848  ( 8 * 80 = 640 bytes)
///
///       // Total: 1488 bytes
///   };
///
/// Person 4 creates one UniformBuffer of sizeof(LightBlock) and calls
/// Upload(&block, sizeof(block)) then BindToSlot(1) each frame.
///
struct LightBlock {
    // Ambient (16 bytes)
    glm::vec3  ambientColor;
    float      ambientIntensity;

    // Counts + presence flags (16 bytes)
    int32_t    numPointLights;
    int32_t    numSpotLights;
    int32_t    hasDirectional;  // 1 if a directional light is active, 0 otherwise
    float      _pad0;

    // Directional light (48 bytes)
    GpuDirectionalLight directional;

    // Point light array (kGpuMaxPointLights * 48 bytes = 768 bytes)
    GpuPointLight pointLights[kGpuMaxPointLights];

    // Spot light array (kGpuMaxSpotLights * 80 bytes = 640 bytes)
    GpuSpotLight spotLights[kGpuMaxSpotLights];

    /// Converts a LightEnvironment to a GPU-ready LightBlock.
    /// Called once per frame by Person 4 before uploading to the UBO.
    static LightBlock Pack(const LightEnvironment& env);
};

static_assert(sizeof(LightBlock) ==
    16                                          // ambient
    + 16                                        // counts
    + sizeof(GpuDirectionalLight)               // 48
    + sizeof(GpuPointLight) * kGpuMaxPointLights // 768
    + sizeof(GpuSpotLight)  * kGpuMaxSpotLights, // 640
    "LightBlock size does not match expected layout — check struct padding");
