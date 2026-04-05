#pragma once

#include "core/MeshData.h"
#include "core/SubMesh.h"
#include <cstdint>
#include <string>

/// GPU mesh with one or more independently drawable submeshes.
///
/// Constructed from a MeshData upload. The MeshData can be discarded
/// after construction — all data lives on the GPU.
///
/// The implementation is backend-specific (src/opengl/core/Mesh.cpp).
/// This header contains no GL types and is safe to include anywhere.
class Mesh {
public:
    explicit Mesh(const MeshData& data);

    ~Mesh();

    Mesh(const Mesh&)            = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&&)                 noexcept;
    Mesh& operator=(Mesh&&)      noexcept;

    /// Draws one submesh. Caller binds shader and material first.
    void DrawSubMesh(uint32_t index) const;

    /// Draws all submeshes in order.
    void DrawAll() const;

    [[nodiscard]] uint32_t           SubMeshCount()     const;
    [[nodiscard]] const SubMesh&     GetSubMesh(uint32_t i) const;
    [[nodiscard]] uint32_t           VertexCount()      const;
    [[nodiscard]] uint32_t           IndexCount()       const;
    [[nodiscard]] bool               IsValid()          const;
    [[nodiscard]] const std::string& GetName()          const;

private:
    struct Impl;
    Impl* m_impl = nullptr;
};
