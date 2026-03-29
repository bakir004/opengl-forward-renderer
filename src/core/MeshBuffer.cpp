#include "MeshBuffer.h"

#include <cassert>
#include <spdlog/spdlog.h>

MeshBuffer::MeshBuffer(const void* vertexData,
                       GLsizeiptr vertexBufferSize,
                       GLsizei vertexCount,
                       const uint32_t* indexData,
                       GLsizei indexCount,
                       const VertexLayout& layout,
                       GLenum usage)
    : m_vbo(Buffer::VERTEX, vertexData, vertexBufferSize, usage),
      m_vertexCount(vertexCount),
      m_indexCount(indexCount),
      m_isIndexed(indexData != nullptr && indexCount > 0)
{
    assert(vertexData != nullptr && "MeshBuffer: vertexData must not be null");
    assert(vertexBufferSize > 0 && "MeshBuffer: vertexBufferSize must be greater than 0");
    assert(vertexCount > 0 && "MeshBuffer: vertexCount must be greater than 0");

    // Either both indexData + indexCount are valid, or neither is used.
    assert(((indexData != nullptr) && (indexCount > 0)) ||
           ((indexData == nullptr) && (indexCount == 0)));

    if (m_isIndexed)
    {
        m_ebo.emplace(
            Buffer::ELEMENT,
            indexData,
            static_cast<GLsizeiptr>(indexCount * sizeof(uint32_t)),
            usage);
    }

    // Bind VAO first so vertex attrib state and optional EBO binding get recorded into it.
    m_vao.Bind();

    m_vbo.Bind();

    if (m_isIndexed)
    {
        m_ebo->Bind();
    }

    // Assumes VertexLayout::Apply() expects the correct VAO + VBO to already be bound.
    layout.Apply();

    // Only unbind the VAO. EBO binding is VAO state.
    m_vao.Unbind();

    if (m_isIndexed)
    {
        spdlog::info("MeshBuffer created successfully (indexed, {} vertices, {} indices)",
                     m_vertexCount, m_indexCount);
    }
    else
    {
        spdlog::info("MeshBuffer created successfully (non-indexed, {} vertices)",
                     m_vertexCount);
    }
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
    if (m_vertexCount <= 0)
    {
        spdlog::warn("MeshBuffer::Draw() called with no vertices");
        return;
    }

    m_vao.Bind();

    if (m_isIndexed)
    {
        if (m_indexCount <= 0)
        {
            spdlog::warn("MeshBuffer::Draw() called with indexed mode but no indices");
            return;
        }

        glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
    }
    else
    {
        glDrawArrays(GL_TRIANGLES, 0, m_vertexCount);
    }
}