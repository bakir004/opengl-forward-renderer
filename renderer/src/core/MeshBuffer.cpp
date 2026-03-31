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
        m_ebo->Bind();

    layout.Apply();

    // Only unbind the VAO. EBO binding is VAO state — unbinding the EBO here would detach it.
    m_vao.Unbind();

    if (m_isIndexed)
    {
        spdlog::info("[MeshBuffer] Uploaded indexed mesh: {} vertices, {} indices ({} bytes)",
            m_vertexCount, m_indexCount, vertexBufferSize);
    }
    else
    {
        spdlog::info("[MeshBuffer] Uploaded non-indexed mesh: {} vertices ({} bytes)",
            m_vertexCount, vertexBufferSize);
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
        spdlog::error("[MeshBuffer] Draw() called with vertex count 0 — mesh was never uploaded or was moved from");
        return;
    }

    m_vao.Bind();

    if (m_isIndexed)
    {
        if (m_indexCount <= 0)
        {
            spdlog::error("[MeshBuffer] Draw() called in indexed mode but index count is 0 — mesh state is corrupt");
            return;
        }

        glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
    }
    else
    {
        glDrawArrays(GL_TRIANGLES, 0, m_vertexCount);
    }
}
