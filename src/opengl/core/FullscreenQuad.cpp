#include "core/FullscreenQuad.h"
#include "core/MeshBuffer.h"
#include "core/VertexLayout.h"
#include <vector>
#include <glad/glad.h>

namespace
{
    struct VertexPos
    {
        float x, y;
    };
}

FullscreenQuad::FullscreenQuad()
{
    // Create a simple quad covering the entire normalized device coordinate space.
    // We use triangle strip topology: (-1,-1), (+1,-1), (-1,+1), (+1,+1)
    // This creates two triangles without needing an index buffer.
    std::vector<VertexPos> vertices = {
        {-1.0f, -1.0f},
        {+1.0f, -1.0f},
        {-1.0f, +1.0f},
        {+1.0f, +1.0f}};

    // Set up vertex layout: 2D position at location 0
    VertexLayout layout({{0, 2, GL_FLOAT, GL_FALSE}});

    // Create the mesh buffer (non-indexed, 4 vertices)
    m_mesh = std::make_shared<MeshBuffer>(
        vertices.data(),
        vertices.size() * sizeof(VertexPos),
        static_cast<GLsizei>(vertices.size()),
        nullptr, // no indices
        0,       // no index count
        layout,
        GL_STATIC_DRAW);
}

FullscreenQuad::~FullscreenQuad() = default;

void FullscreenQuad::Draw() const
{
    if (m_mesh)
    {
        m_mesh->Draw(GL_TRIANGLE_STRIP);
    }
}
