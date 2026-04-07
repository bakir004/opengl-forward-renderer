#pragma once

#include "Buffer.h"
#include "VertexArray.h"
#include "VertexLayout.h"

#include <cstdint>
#include <optional>
#include <glad/glad.h>

/// GPU-side mesh abstraction.
///
/// MeshBuffer owns and manages the OpenGL objects needed to draw geometry:
/// - one VAO (Vertex Array Object)
/// - one VBO (Vertex Buffer Object)
/// - optionally one EBO (Element Buffer Object)
///
/// Supports both:
/// - non-indexed rendering via glDrawArrays
/// - indexed rendering via glDrawElements
class MeshBuffer
{
public:
    /// Creates a GPU mesh from CPU-side vertex and optional index data.
    ///
    /// @param vertexData        Pointer to raw vertex data in CPU memory
    /// @param vertexBufferSize  Total size of the vertex buffer in bytes
    /// @param vertexCount       Number of vertices in the VBO
    /// @param indexData         Pointer to index data (optional, may be nullptr)
    /// @param indexCount        Number of indices (0 for non-indexed meshes)
    /// @param layout            Vertex layout describing how one vertex is packed in memory
    /// @param usage             OpenGL usage hint (default: GL_STATIC_DRAW)
    ///
    /// @note The index type is assumed to be uint32_t / GL_UNSIGNED_INT.
    MeshBuffer(const void* vertexData,
               GLsizeiptr vertexBufferSize,
               GLsizei vertexCount,
               const uint32_t* indexData,
               GLsizei indexCount,
               const VertexLayout& layout,
               GLenum usage = GL_STATIC_DRAW);

    ~MeshBuffer() = default;

    MeshBuffer(const MeshBuffer&) = delete;
    MeshBuffer& operator=(const MeshBuffer&) = delete;

    MeshBuffer(MeshBuffer&&) noexcept = default;
    MeshBuffer& operator=(MeshBuffer&&) noexcept = default;

    void Bind() const;
    void Unbind() const;

    /// Draws the mesh with a chosen primitive topology.
    ///
    /// - indexed   -> glDrawElements(primitiveMode, ...)
    /// - non-indexed -> glDrawArrays(primitiveMode, ...)
    void Draw(GLenum primitiveMode = GL_TRIANGLES) const;

    [[nodiscard]] GLsizei GetVertexCount() const { return m_vertexCount; }
    [[nodiscard]] GLsizei GetIndexCount() const { return m_indexCount; }
    [[nodiscard]] bool IsIndexed() const { return m_isIndexed; }

private:
    VertexArray m_vao;
    Buffer m_vbo;
    std::optional<Buffer> m_ebo;

    GLsizei m_vertexCount = 0;
    GLsizei m_indexCount = 0;
    bool m_isIndexed = false;
};