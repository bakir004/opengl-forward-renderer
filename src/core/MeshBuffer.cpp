#include "MeshBuffer.h"

#include <cassert>
#include <spdlog/spdlog.h>

MeshBuffer::MeshBuffer(const void *vertexData,
                       GLsizeiptr vertexBufferSize,
                       const uint32_t *indexData,
                       GLsizei indexCount,
                       const VertexLayout &layout,
                       GLenum usage)
    : m_vbo(Buffer::VERTEX, vertexData, vertexBufferSize, usage),
      m_ebo(Buffer::ELEMENT,
            indexData,
            static_cast<GLsizeiptr>(indexCount * sizeof(uint32_t)),
            usage),
      m_indexCount(indexCount)
{
    assert(vertexData != nullptr && "MeshBuffer: vertexData must not be null");
    assert(indexData != nullptr && "MeshBuffer: indexData must not be null");
    assert(vertexBufferSize > 0 && "MeshBuffer: vertexBufferSize must be greater than 0");
    assert(indexCount > 0 && "MeshBuffer: indexCount must be greater than 0");

    // Bind VAO first so that:
    // - the EBO binding becomes part of VAO state
    // - vertex attribute setup is recorded into this VAO
    m_vao.Bind();

    // Bind uploaded buffers to the currently active VAO/context.
    m_vbo.Bind();
    m_ebo.Bind();

    // Configure glVertexAttribPointer / glEnableVertexAttribArray according to layout.
    // This assumes VertexLayout::Apply() expects the correct VAO + VBO to already be bound.
    layout.Apply();

    // Unbind only the VAO. We deliberately do not unbind the EBO here,
    // because GL_ELEMENT_ARRAY_BUFFER binding belongs to the VAO state.
    m_vao.Unbind();

    spdlog::info("MeshBuffer created successfully ({} indices)", m_indexCount);
}

void MeshBuffer::Bind() const
{
    m_vao.Bind();
}

void MeshBuffer::Unbind() const
{
    m_vao.Unbind();
}

void MeshBuffer::Draw() const
{
    if (m_indexCount <= 0)
    {
        spdlog::warn("MeshBuffer::Draw() called with no indices");
        return;
    }

    m_vao.Bind();
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
}