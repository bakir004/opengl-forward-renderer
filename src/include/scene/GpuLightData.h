#pragma once

#include <glm/glm.hpp>
#include <cstdint>

/// GPU-side representations of lights, packed for std140 UBO upload.
///
/// std140 alignment quick-reference (the rules that bite):
///   vec3  → occupies 12 bytes but the slot is 16 — always follow vec3 with a float.
///   bool  → must be encoded as uint32_t; GL has no bool scalar in std140.
///   array → every element is individually rounded up to vec4 size (16 bytes).
///   struct array → each element padded to a multiple of its largest member alignment.
///
/// Every struct below is verified with static_assert so mismatches fail at compile time,
/// not silently at runtime with corrupted lighting.
///
/// These are internal to the renderer backend. Scene code works with the CPU types
/// in Light.h; Person 4 converts them via LightBlock::Pack() before upload.

// ---------------------------------------------------------------------------
//  GpuDirectionalLight  (48 bytes)
// ---------------------------------------------------------------------------
//
//  offset  0 : vec3  direction   (12 bytes)
//  offset 12 : float intensity   ( 4 bytes)  — fills vec3 padding
//  offset 16 : vec3  color       (12 bytes)
//  offset 28 : uint  enabled     ( 4 bytes)  — fills vec3 padding
//  offset 32 : uint  castShadow  ( 4 bytes)
//  offset 36 : float depthBias   ( 4 bytes)
//  offset 40 : float normalBias  ( 4 bytes)
//  offset 44 : float _pad        ( 4 bytes)  — round to 16-byte boundary
//
struct GpuDirectionalLight {
    glm::vec3  direction;
    float      intensity;
    glm::vec3  color;
    uint32_t   enabled;
    uint32_t   castShadow;
    float      depthBias;
    float      normalBias;
    float      _pad0;
};
static_assert(sizeof(GpuDirectionalLight) == 48,
    "GpuDirectionalLight std140 size must be 48 bytes");
static_assert(alignof(GpuDirectionalLight) == 4,
    "GpuDirectionalLight must not introduce alignment > 4 that would break array packing");

// ---------------------------------------------------------------------------
//  GpuPointLight  (48 bytes)
// ---------------------------------------------------------------------------
//
//  offset  0 : vec3  position    (12 bytes)
//  offset 12 : float intensity   ( 4 bytes)
//  offset 16 : vec3  color       (12 bytes)
//  offset 28 : float radius      ( 4 bytes)
//  offset 32 : float attn.const  ( 4 bytes)
//  offset 36 : float attn.lin    ( 4 bytes)
//  offset 40 : float attn.quad   ( 4 bytes)
//  offset 44 : uint  enabled     ( 4 bytes)
//
struct GpuPointLight {
    glm::vec3  position;
    float      intensity;
    glm::vec3  color;
    float      radius;
    float      attnConstant;
    float      attnLinear;
    float      attnQuadratic;
    uint32_t   enabled;
};
static_assert(sizeof(GpuPointLight) == 48,
    "GpuPointLight std140 size must be 48 bytes");

// ---------------------------------------------------------------------------
//  GpuSpotLight  (80 bytes)
// ---------------------------------------------------------------------------
//
//  offset  0 : vec3  position    (12 bytes)
//  offset 12 : float intensity   ( 4 bytes)
//  offset 16 : vec3  direction   (12 bytes)
//  offset 28 : float radius      ( 4 bytes)
//  offset 32 : vec3  color       (12 bytes)
//  offset 44 : float innerCos    ( 4 bytes)  — pre-computed cos(innerDeg)
//  offset 48 : float outerCos    ( 4 bytes)  — pre-computed cos(outerDeg)
//  offset 52 : float attn.const  ( 4 bytes)
//  offset 56 : float attn.lin    ( 4 bytes)
//  offset 60 : float attn.quad   ( 4 bytes)
//  offset 64 : uint  enabled     ( 4 bytes)
//  offset 68 : float _pad[3]     (12 bytes)  — round to 16-byte boundary
//
struct GpuSpotLight {
    glm::vec3  position;
    float      intensity;
    glm::vec3  direction;
    float      radius;
    glm::vec3  color;
    float      innerCos;    // cos(innerDeg) — cheaper than computing in fragment shader
    float      outerCos;    // cos(outerDeg)
    float      attnConstant;
    float      attnLinear;
    float      attnQuadratic;
    uint32_t   enabled;
    float      _pad0;
    float      _pad1;
    float      _pad2;
};
static_assert(sizeof(GpuSpotLight) == 80,
    "GpuSpotLight std140 size must be 80 bytes");
