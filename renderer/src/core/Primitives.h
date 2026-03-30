// Primitives.h
#pragma once

#include "MeshBuffer.h"
#include "VertexLayout.h"

#include <glad/glad.h>
#include <glm/vec3.hpp>

#include <cstdint>
#include <type_traits>
#include <vector>

/// Interleaved vertex format used by the built-in test primitives.
/// Matches shaders/basic.vert:
/// - location 0: position (vec3)
/// - location 1: color    (vec3)
struct VertexPC
{
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 color{1.0f, 1.0f, 1.0f};
};

static_assert(std::is_standard_layout_v<VertexPC>, "VertexPC must be standard layout.");
static_assert(sizeof(VertexPC) == sizeof(float) * 6, "VertexPC must be tightly packed as 6 floats.");

/// CPU-side mesh data for a primitive.
/// Can represent both:
/// - non-indexed geometry (indices empty)
/// - indexed geometry     (indices filled)
struct PrimitiveMeshData
{
    std::vector<VertexPC> vertices;
    std::vector<uint32_t> indices;

    [[nodiscard]] GLsizeiptr GetVertexBufferSize() const;
    [[nodiscard]] GLsizei GetVertexCount() const;
    [[nodiscard]] GLsizei GetIndexCount() const;
    [[nodiscard]] bool IsIndexed() const;

    [[nodiscard]] static VertexLayout CreateLayout();
    [[nodiscard]] MeshBuffer CreateMeshBuffer(GLenum usage = GL_STATIC_DRAW) const;
};

/// Generates a single triangle with per-vertex colors.
/// Intended to test the non-indexed rendering path.
[[nodiscard]] PrimitiveMeshData GenerateTriangle();

/// Generates a quad centered at the origin using indexed rendering.
[[nodiscard]] PrimitiveMeshData GenerateQuad();

/// Generates a unit cube centered at the origin with unique colors per face.
[[nodiscard]] PrimitiveMeshData GenerateCube();