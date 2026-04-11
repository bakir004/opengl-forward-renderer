#include "core/Mesh.h"
#include "core/Buffer.h"
#include "core/MeshData.h"
#include "core/VertexArray.h"
#include "core/VertexLayout.h"
#include <glad/glad.h>
#include <spdlog/spdlog.h>
#include <cassert>
#include <vector>

// ---------------------------------------------------------------------------
//  Impl — fully defined only in this translation unit
// ---------------------------------------------------------------------------

struct Mesh::Impl {
    VertexArray          vao;
    Buffer               vbo;
    Buffer               ebo;
    std::vector<SubMesh> submeshes;
    uint32_t             vertexCount = 0;
    uint32_t             indexCount  = 0;
    std::string          name;

    Impl(const MeshData& data, GLenum usage)
        : vbo(Buffer::VERTEX,
              data.vertices.data(),
              static_cast<GLsizeiptr>(data.vertices.size()) * sizeof(VertexPNT),
              usage)
        , ebo(Buffer::ELEMENT,
              data.indices.data(),
              static_cast<GLsizeiptr>(data.indices.size()) * sizeof(uint32_t),
              usage)
        , submeshes(data.submeshes)
        , vertexCount(data.VertexCount())
        , indexCount(data.IndexCount())
        , name(data.name)
    {
        VertexLayout layout({
            { 0, 3, GL_FLOAT, GL_FALSE },  // position
            { 1, 3, GL_FLOAT, GL_FALSE },  // normal
            { 2, 2, GL_FLOAT, GL_FALSE },  // uv
        });

        vao.Bind();
        vbo.Bind();
        ebo.Bind();
        layout.Apply();
        vao.Unbind();

        spdlog::info("[Mesh] Uploaded '{}': {} vertices, {} indices, {} submesh(es)",
                     name, vertexCount, indexCount, submeshes.size());

        for (uint32_t i = 0; i < submeshes.size(); ++i) {
            spdlog::debug("[Mesh]   [{}] name='{}' indexByteOffset={} "
                          "indexCount={} baseVertex={} material={}",
                          i,
                          submeshes[i].name,
                          submeshes[i].indexByteOffset,
                          submeshes[i].indexCount,
                          submeshes[i].baseVertex,
                          submeshes[i].materialIndex);
        }
    }
};

// ---------------------------------------------------------------------------
//  Mesh — forwards everything to Impl
// ---------------------------------------------------------------------------

Mesh::Mesh(const MeshData& data)
    : m_impl(new Impl(data, GL_STATIC_DRAW))
{
    assert(data.IsValid() && "Mesh: MeshData must be valid before upload");
}

Mesh::~Mesh() {
    delete m_impl;
}

Mesh::Mesh(Mesh&& other) noexcept
    : m_impl(other.m_impl)
{
    other.m_impl = nullptr;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        delete m_impl;
        m_impl       = other.m_impl;
        other.m_impl = nullptr;
    }
    return *this;
}

void Mesh::DrawSubMesh(uint32_t i) const {
    if (!m_impl || i >= m_impl->submeshes.size()) {
        spdlog::warn("[Mesh] DrawSubMesh({}) out of range", i);
        return;
    }
    const SubMesh& sm = m_impl->submeshes[i];
    m_impl->vao.Bind();
    glDrawElementsBaseVertex(
        GL_TRIANGLES,
        static_cast<GLsizei>(sm.indexCount),
        GL_UNSIGNED_INT,
        reinterpret_cast<const void*>(static_cast<uintptr_t>(sm.indexByteOffset)),
        sm.baseVertex
    );
}

void Mesh::DrawAll() const {
    if (!m_impl) return;
    m_impl->vao.Bind();
    for (const SubMesh& sm : m_impl->submeshes) {
        glDrawElementsBaseVertex(
            GL_TRIANGLES,
            static_cast<GLsizei>(sm.indexCount),
            GL_UNSIGNED_INT,
            reinterpret_cast<const void*>(static_cast<uintptr_t>(sm.indexByteOffset)),
            sm.baseVertex
        );
    }
}

uint32_t Mesh::SubMeshCount() const {
    return m_impl ? static_cast<uint32_t>(m_impl->submeshes.size()) : 0;
}

const SubMesh& Mesh::GetSubMesh(uint32_t i) const {
    return m_impl->submeshes[i];
}

uint32_t Mesh::VertexCount() const {
    return m_impl ? m_impl->vertexCount : 0;
}

uint32_t Mesh::IndexCount() const {
    return m_impl ? m_impl->indexCount : 0;
}

bool Mesh::IsValid() const {
    return m_impl && m_impl->vertexCount > 0;
}

const std::string& Mesh::GetName() const {
    static const std::string empty;
    return m_impl ? m_impl->name : empty;
}