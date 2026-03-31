// SampleScene.cpp — Sprint 2 sample scene.

#include "SampleScene.h"

#include "core/MeshBuffer.h"
#include "core/Primitives.h"
#include "core/Renderer.h"
#include "core/ShaderProgram.h"

#include <spdlog/spdlog.h>

namespace {

void PlaceInClipSpace(PrimitiveMeshData& mesh, glm::vec3 translate, float uniformScale) {
    for (auto& v : mesh.vertices)
        v.position = translate + uniformScale * v.position;
}

} // namespace

bool SampleScene::Setup(const std::string& vertexShaderPath, const std::string& fragmentShaderPath) {
    m_ready = false;
    m_shader.reset();
    m_triangle.reset();
    m_quad.reset();
    m_cube.reset();
    m_sphere.reset();

    spdlog::info("[SampleScene] Setting up with shaders: '{}' + '{}'", vertexShaderPath, fragmentShaderPath);

    m_shader = std::make_unique<ShaderProgram>(vertexShaderPath, fragmentShaderPath);
    if (!m_shader->IsValid()) {
        spdlog::error("[SampleScene] Shader program failed to compile/link — scene cannot render without a valid shader");
        return false;
    }

    constexpr float kXLeft     = -0.58f;
    constexpr float kXCenter   = 0.0f;
    constexpr float kXRight    = 0.58f;
    constexpr float kYTop      = 0.40f;
    constexpr float kYBottom   = -0.40f;
    constexpr float kScale2D   = 0.40f;
    constexpr float kScaleCube = 0.32f;

    auto tri = GenerateTriangle();
    PlaceInClipSpace(tri, glm::vec3(kXLeft, kYTop, 0.0f), kScale2D);
    m_triangle = std::make_unique<MeshBuffer>(tri.CreateMeshBuffer());

    auto quad = GenerateQuad();
    PlaceInClipSpace(quad, glm::vec3(kXCenter, kYTop, 0.0f), kScale2D);
    m_quad = std::make_unique<MeshBuffer>(quad.CreateMeshBuffer());

    auto cube = GenerateCube();
    PlaceInClipSpace(cube, glm::vec3(kXRight, kYTop, 0.0f), kScaleCube);
    m_cube = std::make_unique<MeshBuffer>(cube.CreateMeshBuffer());

    auto sphere = GenerateSphere(0.28f, 16);
    PlaceInClipSpace(sphere, glm::vec3(kXCenter, kYBottom, 0.0f), 1.0f);
    m_sphere = std::make_unique<MeshBuffer>(sphere.CreateMeshBuffer());

    m_ready = true;
    spdlog::info("[SampleScene] Ready — triangle, quad, cube, sphere");
    return true;
}

void SampleScene::Render(Renderer& renderer, float /*timeSeconds*/) {
    if (!m_ready) return;

    renderer.SetDepthTest(true, DepthFunc::Less);
    renderer.SetCullMode(CullMode::Back);

    renderer.SubmitDraw(*m_shader, *m_triangle);
    renderer.SubmitDraw(*m_shader, *m_quad);
    renderer.SubmitDraw(*m_shader, *m_cube);
    renderer.SubmitDraw(*m_shader, *m_sphere);

    renderer.UnbindShader();
}
