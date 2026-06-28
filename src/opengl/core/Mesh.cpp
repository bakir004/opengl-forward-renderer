#include "core/Mesh.h"
#include "core/Buffer.h"
#include "core/MeshData.h"
#include "core/VertexArray.h"
#include "core/VertexLayout.h"
#include <glad/glad.h>
#include <spdlog/spdlog.h>
#include <cassert>
#include <vector>

namespace
{

AABB ComputeVertexBounds(const std::vector<VertexPNT>& vertices)
{
    AABB bounds = AABB::Empty();
    for (const VertexPNT& vertex : vertices)
        bounds.Expand(vertex.position);
    return bounds;
}

AABB ComputeSubMeshBounds(const MeshData& data, const SubMesh& subMesh)
{
    if (subMesh.localBounds.IsValid())
        return subMesh.localBounds;

    AABB bounds = AABB::Empty();
    const uint32_t firstIndex = subMesh.indexByteOffset / sizeof(uint32_t);
    for (uint32_t i = 0; i < subMesh.indexCount; ++i)
    {
        const uint32_t vertexIndex = data.indices[firstIndex + i] + static_cast<uint32_t>(subMesh.baseVertex);
        if (vertexIndex < data.vertices.size())
            bounds.Expand(data.vertices[vertexIndex].position);
    }
    return bounds;
}

} // namespace

// ---------------------------------------------------------------------------
//  Impl — fully defined only in this translation unit
// ---------------------------------------------------------------------------

struct Mesh::Impl {
    VertexArray          vao;
    Buffer               vbo;
    Buffer               ebo;
    std::vector<SubMesh> submeshes;
    std::vector<AABB>    submeshBounds;
    AABB                 localBounds = AABB::Empty();
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
        , localBounds(ComputeVertexBounds(data.vertices))
        , vertexCount(data.VertexCount())
        , indexCount(data.IndexCount())
        , name(data.name)
    {
        submeshBounds.reserve(submeshes.size());
        for (const SubMesh& subMesh : submeshes)
            submeshBounds.push_back(ComputeSubMeshBounds(data, subMesh));
        VertexLayout layout({
            { 0, 3, GL_FLOAT, GL_FALSE },  // position
            { 1, 3, GL_FLOAT, GL_FALSE },  // normal
            { 2, 2, GL_FLOAT, GL_FALSE }, //uv
            { 3, 4, GL_FLOAT, GL_FALSE }  // tangent
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

void Mesh::SetInstanceTransformBuffer(uint32_t bufferId) const {
    if (!m_impl || bufferId == 0) return;

    constexpr GLuint firstLocation = 6;
    constexpr GLsizei columnSize = 4 * sizeof(float);

    m_impl->vao.Bind();
    glBindBuffer(GL_ARRAY_BUFFER, bufferId);

    for (GLuint column = 0; column < 4; ++column) {
        const GLuint location = firstLocation + column;
        glEnableVertexAttribArray(location);
        glVertexAttribPointer(
            location,
            4,
            GL_FLOAT,
            GL_FALSE,
            sizeof(glm::mat4),
            reinterpret_cast<const void*>(
                static_cast<uintptr_t>(column * columnSize)));
        glVertexAttribDivisor(location, 1);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    m_impl->vao.Unbind();
}

void Mesh::DrawSubMeshInstanced(uint32_t i, uint32_t instanceCount) const {
    if (!m_impl || i >= m_impl->submeshes.size() || instanceCount == 0) return;
    const SubMesh& sm = m_impl->submeshes[i];
    m_impl->vao.Bind();
    glDrawElementsInstancedBaseVertex(
        GL_TRIANGLES,
        static_cast<GLsizei>(sm.indexCount),
        GL_UNSIGNED_INT,
        reinterpret_cast<const void*>(static_cast<uintptr_t>(sm.indexByteOffset)),
        static_cast<GLsizei>(instanceCount),
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

AABB Mesh::GetLocalBounds() const {
    return m_impl ? m_impl->localBounds : AABB::Empty();
}

AABB Mesh::GetSubMeshBounds(uint32_t index) const {
    if (!m_impl || index >= m_impl->submeshBounds.size())
        return AABB::Empty();
    return m_impl->submeshBounds[index];
}
