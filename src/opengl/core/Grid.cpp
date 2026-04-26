#include "core/Grid.h"
#include "core/Primitives.h"
#include "core/ShaderProgram.h"
#include "core/MeshBuffer.h"
#include "core/VertexLayout.h"
#include "assets/AssetImporter.h"

#include <glad/glad.h>
#include <spdlog/spdlog.h>

#include "core/Primitives.h"

Grid::Grid() {
    // Load grid shader
    m_shader = AssetImporter::LoadShader("assets/shaders/grid.vert", "assets/shaders/grid.frag");

    // Create fullscreen quad in NDC space (clip space)
    // This quad will be unprojected in the vertex shader to create the infinite grid
    std::vector<VertexPC> vertices = {
        {{-1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
        {{ 1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
        {{ 1.0f,  1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
        {{-1.0f,  1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}
    };

    std::vector<uint32_t> indices = {
        0, 1, 2,
        2, 3, 0
    };

    PrimitiveMeshData meshData;
    meshData.vertices = std::move(vertices);
    meshData.indices = std::move(indices);

    m_quadMesh = std::make_shared<MeshBuffer>(meshData.CreateMeshBuffer());

    spdlog::info("Grid initialized");
}

void Grid::Draw() const {
    if (!m_shader || !m_shader->IsValid()) {
        spdlog::warn("Grid shader is not valid, skipping draw");
        return;
    }

    // Enable blending for grid transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Enable depth test but don't write to depth buffer initially
    // This allows the grid to be rendered behind objects
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    m_shader->Bind();

    // Set grid parameters
    m_shader->SetUniform("u_GridScale", m_gridScale);
    m_shader->SetUniform("u_GridMinorScale", m_gridMinorScale);
    m_shader->SetUniform("u_FadeDistance", m_fadeDistance);
    m_shader->SetUniform("u_AxisThickness", m_axisThickness);

    // Draw the fullscreen quad
    m_quadMesh->Draw();

    ShaderProgram::Unbind();

    // Restore depth mask
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}
