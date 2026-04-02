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

} // namespace for hiding this helper from external use

bool SampleScene::Setup(const std::string& vertexShaderPath, const std::string& fragmentShaderPath) {
    m_ready = false;
    m_playerPosition = glm::vec3(1.2f, 0.0f, -4.0f); // separate movable cube start
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

void SampleScene::Render(FrameSubmission& submission, float /*timeSeconds*/) {
    if (!m_ready) return;

    submission.clearColor = {0.08f, 0.09f, 0.12f, 1.0f};
    submission.clearFlags = ClearFlags::ColorDepth;
    submission.viewport = Viewport{0, 0, submission.viewport.width, submission.viewport.height};

    // Add items for the four meshes.
    RenderItem triItem;
    triItem.mesh = m_triangle.get();
    triItem.shader = m_shader.get();
    triItem.transform.SetTranslation({-1.2f, 0.0f, -3.0f});

    RenderItem quadItem;
    quadItem.mesh = m_quad.get();
    quadItem.shader = m_shader.get();
    quadItem.transform.SetTranslation({-0.4f, 0.0f, -3.0f});

    RenderItem staticCubeItem;
    staticCubeItem.mesh = m_cube.get();
    staticCubeItem.shader = m_shader.get();
    staticCubeItem.transform.SetTranslation({0.4f, 0.0f, -3.0f});

    RenderItem playerCubeItem;
    playerCubeItem.mesh = m_cube.get();
    playerCubeItem.shader = m_shader.get();
    playerCubeItem.transform.SetTranslation(m_playerPosition);

    RenderItem sphereItem;
    sphereItem.mesh = m_sphere.get();
    sphereItem.shader = m_shader.get();
    sphereItem.transform.SetTranslation({1.2f, 0.0f, -3.0f});

    RenderItem anotherCube;
    anotherCube.mesh = m_cube.get();
    anotherCube.shader = m_shader.get();
    anotherCube.transform.SetTranslation({-2.0f, 0.0f, -4.0f});
    anotherCube.transform.SetScale({0.5f, 0.5f, 0.5f});

    RenderItem farCube;
    farCube.mesh = m_cube.get();
    farCube.shader = m_shader.get();
    farCube.transform.SetTranslation({1.5f, 0.5f, -6.0f});
    farCube.transform.SetScale({0.7f, 0.7f, 0.7f});

    submission.objects.clear();
    submission.objects.push_back(triItem);
    submission.objects.push_back(quadItem);
    submission.objects.push_back(staticCubeItem);
    submission.objects.push_back(playerCubeItem);
    submission.objects.push_back(sphereItem);
    submission.objects.push_back(anotherCube);
    submission.objects.push_back(farCube);
}

void SampleScene::SetPlayerPosition(const glm::vec3& position) {
    m_playerPosition = position;
}
