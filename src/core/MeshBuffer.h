#pragma once

#include "Buffer.h"
#include "VertexArray.h"
#include "VertexLayout.h"

#include <cstdint>
#include <glad/glad.h>

/// GPU-side mesh abstraction.
///
/// MeshBuffer owns and manages all OpenGL objects needed to draw indexed geometry:
/// - one VAO (Vertex Array Object)
/// - one VBO (Vertex Buffer Object)
/// - one EBO (Element Buffer Object)
///
/// Its job is to take CPU-side mesh data (vertices + indices), upload it to the GPU,
/// configure vertex attribute bindings through a VertexLayout, and expose a simple
/// Bind()/Draw() interface.
///
/// This class is intentionally focused only on geometry storage/binding.
/// It does not know anything about shaders, materials, textures, lights, or transforms.
class MeshBuffer
{
public:
    /// Creates a GPU mesh from CPU-side vertex and index data.
    ///
    /// Expected setup flow:
    /// 1. Create VAO/VBO/EBO
    /// 2. Upload vertex data into the VBO
    /// 3. Upload index data into the EBO
    /// 4. Apply the provided VertexLayout while VAO and VBO are bound
    ///
    /// @param vertexData        Pointer to raw vertex data in CPU memory
    /// @param vertexBufferSize  Total size of the vertex buffer in bytes
    /// @param indexData         Pointer to index data (typically uint32_t array)
    /// @param indexCount        Number of indices to draw
    /// @param layout            Vertex layout describing how one vertex is packed in memory
    /// @param usage             OpenGL usage hint (default: GL_STATIC_DRAW)
    ///
    /// @note The index type is assumed to be uint32_t / GL_UNSIGNED_INT.
    MeshBuffer(const void *vertexData,
               GLsizeiptr vertexBufferSize,
               const uint32_t *indexData,
               GLsizei indexCount,
               const VertexLayout &layout,
               GLenum usage = GL_STATIC_DRAW);

    /// Releases owned GPU resources automatically through RAII member objects.
    ~MeshBuffer() = default;

    MeshBuffer(const MeshBuffer &) = delete;
    MeshBuffer &operator=(const MeshBuffer &) = delete;

    /// Transfers ownership of the underlying GPU resources.
    MeshBuffer(MeshBuffer &&) noexcept = default;
    MeshBuffer &operator=(MeshBuffer &&) noexcept = default;

    /// Binds this mesh's VAO.
    ///
    /// Once bound, the GPU knows:
    /// - which VBO provides vertex data
    /// - which EBO provides index data
    /// - how vertex attributes are laid out
    void Bind() const;

    /// Unbinds this mesh's VAO.
    ///
    /// This only unbinds the VAO. It intentionally does not manually detach the EBO,
    /// because EBO binding is VAO state in OpenGL.
    void Unbind() const;

    /// Draws the mesh as indexed triangles.
    ///
    /// Internally uses:
    /// glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr)
    ///
    /// @note Assumes the correct shader/program is already bound by higher-level code.
    void Draw() const;

    /// Returns the number of indices stored in this mesh.
    [[nodiscard]] GLsizei GetIndexCount() const { return m_indexCount; }

private:
    VertexArray m_vao;
    Buffer m_vbo;
    Buffer m_ebo;
    GLsizei m_indexCount = 0;
};