// Primitives.cpp
#include "Primitives.h"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>

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
        spdlog::error("[Primitives] CreateMeshBuffer() called on empty PrimitiveMeshData — no vertices to upload");
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
        // Back (+Z)
        {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.2f, 0.2f}},
        {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.2f, 0.2f}},
        {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.2f, 0.2f}},
        {{-0.5f,  0.5f,  0.5f}, {1.0f, 0.2f, 0.2f}},

        // Front (-Z, towards screen)
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
PrimitiveMeshData GenerateSphere(float radius, int detail)
{
    const int stacks = std::max(2, detail);
    const int slices = std::max(3, detail * 2);

    PrimitiveMeshData mesh;
    mesh.vertices.reserve(static_cast<size_t>((stacks + 1) * (slices + 1)));
    mesh.indices.reserve(static_cast<size_t>(stacks * slices * 6));

    const float pi = std::atan(1.0f) * 4.0f;

    // Generate vertices ring by ring, top (phi=0) to bottom (phi=PI).
    // Each ring has (slices+1) vertices; the first and last share position
    // but are kept separate so index stitching stays uniform.
    for (int i = 0; i <= stacks; ++i)
    {
        float phi = pi * static_cast<float>(i) / static_cast<float>(stacks);
        float y   = radius * std::cos(phi);
        float r   = radius * std::sin(phi); // ring radius at this latitude

        for (int j = 0; j <= slices; ++j)
        {
            float theta = 2.0f * pi * static_cast<float>(j) / static_cast<float>(slices);
            float x = r * std::cos(theta);
            float z = r * std::sin(theta);

            glm::vec3 pos(x, y, z);
            // Map surface normal (pos/radius) from [-1,1] to [0,1] for color.
            glm::vec3 color = (pos / radius) * 0.5f + glm::vec3(0.5f);
            mesh.vertices.push_back({pos, color});
        }
    }

    // Stitch adjacent rings into quads (two triangles each).
    // vertex [i][j] lives at index i*(slices+1)+j.
    for (int i = 0; i < stacks; ++i)
    {
        for (int j = 0; j < slices; ++j)
        {
            uint32_t a = static_cast<uint32_t>(i * (slices + 1) + j);
            uint32_t b = a + static_cast<uint32_t>(slices + 1);

            mesh.indices.push_back(a);
            mesh.indices.push_back(a + 1);
            mesh.indices.push_back(b);

            mesh.indices.push_back(b);
            mesh.indices.push_back(a + 1);
            mesh.indices.push_back(a);

            mesh.indices.push_back(a + 1);
            mesh.indices.push_back(b + 1);
            mesh.indices.push_back(b);

            mesh.indices.push_back(b);
            mesh.indices.push_back(b + 1);
            mesh.indices.push_back(a + 1);
        }
    }

    return mesh;
}
