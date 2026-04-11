// Primitives.h
#pragma once

#include "MeshBuffer.h"
#include "VertexLayout.h"

#include <glad/glad.h>
#include <glm/vec3.hpp>

#include <cstdint>
#include <type_traits>
#include <vector>

/// Interleaved vertex format used by the built-in primitives.
/// Matches shaders/basic.vert:
///   location 0 — position (vec3)
///   location 1 — color    (vec3)
struct VertexPC {
    glm::vec3 position{ 0.0f, 0.0f, 0.0f };
    glm::vec3 color   { 1.0f, 1.0f, 1.0f };
};

static_assert(std::is_standard_layout_v<VertexPC>,        "VertexPC must be standard layout.");
static_assert(sizeof(VertexPC) == sizeof(float) * 6,      "VertexPC must be tightly packed as 6 floats.");

// ─── Color mode ──────────────────────────────────────────────────────────────

/// Controls how vertex colours are assigned when generating a primitive.
enum class ColorMode {
    /// Each face gets a distinct, visually saturated colour — the default "rainbow" look.
    Rainbow,

    /// Every face uses a slightly varied tint derived from a single base colour.
    /// Faces are distinguishable, but the overall object reads as one colour.
    Solid,
};

// ─── Generator options ───────────────────────────────────────────────────────

/// Options forwarded to every Generate* function.
///
/// Usage (C++20 designated initialisers):
/// @code
///   auto cube = GenerateCube({ .colorMode = ColorMode::Solid,
///                              .baseColor = {0.4f, 0.7f, 1.0f} });
///
///   auto sphere = GenerateSphere(1.0f, 16, { .doubleSided = true });
/// @endcode
struct PrimitiveOptions {
    /// Colour assignment strategy.
    ColorMode colorMode  = ColorMode::Rainbow;

    /// Base colour used when @p colorMode == ColorMode::Solid.
    /// Each face receives a subtle tint variation so the shape reads in 3-D.
    glm::vec3 baseColor  = { 1.0f, 1.0f, 1.0f };

    /// When true, every triangle is duplicated with reversed winding so both
    /// the front AND back face are visible regardless of the active cull mode.
    /// Use this for open surfaces (a plane seen from both sides) or for
    /// wireframe previews where back-face culling would hide half the mesh.
    bool doubleSided = false;
};

// ─── CPU-side mesh data ───────────────────────────────────────────────────────

/// CPU-side mesh data for a primitive. Supports both indexed and non-indexed geometry.
struct PrimitiveMeshData {
    std::vector<VertexPC> vertices;
    std::vector<uint32_t> indices;

    [[nodiscard]] GLsizeiptr GetVertexBufferSize() const;
    [[nodiscard]] GLsizei    GetVertexCount()      const;
    [[nodiscard]] GLsizei    GetIndexCount()       const;
    [[nodiscard]] bool       IsIndexed()           const;

    [[nodiscard]] static VertexLayout CreateLayout();
    [[nodiscard]] MeshBuffer CreateMeshBuffer(GLenum usage = GL_STATIC_DRAW) const;
};

// ─── Generator functions ──────────────────────────────────────────────────────

/// Generates a single equilateral-ish triangle (non-indexed).
[[nodiscard]] PrimitiveMeshData GenerateTriangle(const PrimitiveOptions& opts = {});

/// Generates a unit quad centred at the origin (indexed, 2 triangles).
[[nodiscard]] PrimitiveMeshData GenerateQuad(const PrimitiveOptions& opts = {});

/// Generates a unit cube centred at the origin with one colour per face (indexed, 12 triangles).
[[nodiscard]] PrimitiveMeshData GenerateCube(const PrimitiveOptions& opts = {});

/// Generates a square-based pyramid centred at the origin
/// (base at y = -0.5, apex at y = +0.5, indexed, 6 triangles).
[[nodiscard]] PrimitiveMeshData GeneratePyramid(const PrimitiveOptions& opts = {});

/// Generates a UV sphere centred at the origin (indexed).
///
/// @param radius  Sphere radius in local space.
/// @param detail  Mesh density: stacks = detail, slices = detail * 2.
///                Clamped to a minimum of 2 stacks / 3 slices.
///                detail=4 → low-poly; detail=32 → smooth.
[[nodiscard]] PrimitiveMeshData GenerateSphere(float radius, int detail,
                                               const PrimitiveOptions& opts = {});
