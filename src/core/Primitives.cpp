// Primitives.cpp
#include "Primitives.h"

#include <spdlog/spdlog.h>

GLsizeiptr PrimitiveMeshData::GetVertexBufferSize() const
{
    return static_cast<GLsizeiptr>(vertices.size() * sizeof(VertexPC));
}

GLsizei PrimitiveMeshData::GetVertexCount() const
{
    return static_cast<GLsizei>(vertices.size());
}

GLsizei PrimitiveMeshData::GetIndexCount() const
{
    return static_cast<GLsizei>(indices.size());
}

bool PrimitiveMeshData::IsIndexed() const
{
    return !indices.empty();
}

VertexLayout PrimitiveMeshData::CreateLayout()
{
    return VertexLayout({
        {0, 3, GL_FLOAT, GL_FALSE}, // position
        {1, 3, GL_FLOAT, GL_FALSE}, // color
    });
}

MeshBuffer PrimitiveMeshData::CreateMeshBuffer(GLenum usage) const
{
    if (vertices.empty())
    {
        spdlog::warn("PrimitiveMeshData::CreateMeshBuffer() called with empty vertex data");
    }

    const void* vertexData = vertices.empty() ? nullptr : vertices.data();
    const uint32_t* indexData = indices.empty() ? nullptr : indices.data();

    return MeshBuffer(
        vertexData,
        GetVertexBufferSize(),
        GetVertexCount(),
        indexData,
        GetIndexCount(),
        CreateLayout(),
        usage);
}

PrimitiveMeshData GenerateTriangle()
{
    PrimitiveMeshData mesh;
    mesh.vertices = {
        {{-0.6f, -0.5f, 0.0f}, {1.0f, 0.2f, 0.2f}},
        {{ 0.6f, -0.5f, 0.0f}, {0.2f, 1.0f, 0.2f}},
        {{ 0.0f,  0.6f, 0.0f}, {0.2f, 0.4f, 1.0f}},
    };

    // Non-indexed path test
    mesh.indices = {};

    return mesh;
}

PrimitiveMeshData GenerateQuad()
{
    PrimitiveMeshData mesh;
    mesh.vertices = {
        {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.2f, 0.2f}},
        {{ 0.5f, -0.5f, 0.0f}, {0.2f, 1.0f, 0.2f}},
        {{ 0.5f,  0.5f, 0.0f}, {0.2f, 0.4f, 1.0f}},
        {{-0.5f,  0.5f, 0.0f}, {1.0f, 0.9f, 0.2f}},
    };

    mesh.indices = {
        0, 1, 2,
        2, 3, 0,
    };

    return mesh;
}

PrimitiveMeshData GenerateCube()
{
    PrimitiveMeshData mesh;
    mesh.vertices = {
        // Front (+Z)
        {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.2f, 0.2f}},
        {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.2f, 0.2f}},
        {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.2f, 0.2f}},
        {{-0.5f,  0.5f,  0.5f}, {1.0f, 0.2f, 0.2f}},

        // Back (-Z)
        {{ 0.5f, -0.5f, -0.5f}, {0.2f, 1.0f, 0.2f}},
        {{-0.5f, -0.5f, -0.5f}, {0.2f, 1.0f, 0.2f}},
        {{-0.5f,  0.5f, -0.5f}, {0.2f, 1.0f, 0.2f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.2f, 1.0f, 0.2f}},

        // Left (-X)
        {{-0.5f, -0.5f, -0.5f}, {0.2f, 0.4f, 1.0f}},
        {{-0.5f, -0.5f,  0.5f}, {0.2f, 0.4f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, {0.2f, 0.4f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.2f, 0.4f, 1.0f}},

        // Right (+X)
        {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.9f, 0.2f}},
        {{ 0.5f, -0.5f, -0.5f}, {1.0f, 0.9f, 0.2f}},
        {{ 0.5f,  0.5f, -0.5f}, {1.0f, 0.9f, 0.2f}},
        {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.9f, 0.2f}},

        // Top (+Y)
        {{-0.5f,  0.5f,  0.5f}, {0.9f, 0.2f, 1.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.9f, 0.2f, 1.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.9f, 0.2f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.9f, 0.2f, 1.0f}},

        // Bottom (-Y)
        {{-0.5f, -0.5f, -0.5f}, {0.2f, 1.0f, 1.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {0.2f, 1.0f, 1.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {0.2f, 1.0f, 1.0f}},
        {{-0.5f, -0.5f,  0.5f}, {0.2f, 1.0f, 1.0f}},
    };

    mesh.indices = {
         0,  1,  2,  2,  3,  0,
         4,  5,  6,  6,  7,  4,
         8,  9, 10, 10, 11,  8,
        12, 13, 14, 14, 15, 12,
        16, 17, 18, 18, 19, 16,
        20, 21, 22, 22, 23, 20,
    };

    return mesh;
}