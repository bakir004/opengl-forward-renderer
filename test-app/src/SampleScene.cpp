#include "SampleScene.h"

#include "core/Camera.h"
#include "core/KeyboardInput.h"
#include "core/MouseInput.h"
#include "core/Primitives.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

namespace {

void PlaceInClipSpace(PrimitiveMeshData& mesh, glm::vec3 translate, float uniformScale) {
    for (auto& v : mesh.vertices)
        v.position = translate + uniformScale * v.position;
}

} // namespace

bool SampleScene::Setup() {
    spdlog::info("[SampleScene] Setting up");

    m_shader = std::make_unique<ShaderProgram>("assets/shaders/basic.vert", "assets/shaders/basic.frag");
    if (!m_shader->IsValid()) {
        spdlog::error("[SampleScene] Shader failed to compile/link — aborting setup");
        return false;
    }

    // Upload geometry (PlaceInClipSpace bakes an offset+scale into vertex positions)
    constexpr float kXLeft     = -0.58f;
    constexpr float kXCenter   =  0.0f;
    constexpr float kXRight    =  0.58f;
    constexpr float kYTop      =  0.40f;
    constexpr float kYBottom   = -0.40f;
    constexpr float kScale2D   =  0.40f;
    constexpr float kScaleCube =  0.32f;

    auto tri = GenerateTriangle();
    PlaceInClipSpace(tri, {kXLeft, kYTop, 0.0f}, kScale2D);
    m_triangle = std::make_unique<MeshBuffer>(tri.CreateMeshBuffer());

    auto quad = GenerateQuad();
    PlaceInClipSpace(quad, {kXCenter, kYTop, 0.0f}, kScale2D);
    m_quad = std::make_unique<MeshBuffer>(quad.CreateMeshBuffer());

    auto cube = GenerateCube();
    PlaceInClipSpace(cube, {kXRight, kYTop, 0.0f}, kScaleCube);
    m_cube = std::make_unique<MeshBuffer>(cube.CreateMeshBuffer());

    auto sphere = GenerateSphere(0.28f, 16);
    PlaceInClipSpace(sphere, {kXCenter, kYBottom, 0.0f}, 1.0f);
    m_sphere = std::make_unique<MeshBuffer>(sphere.CreateMeshBuffer());

    // --- Camera ---
    Camera cam;
    cam.SetPosition({0.0f, 1.0f, 5.0f});
    SetCamera(cam);
    SetClearColor({0.08f, 0.09f, 0.12f, 1.0f});

    // --- Objects (declarative) ---
    RenderItem triItem;
    triItem.mesh   = m_triangle.get();
    triItem.shader = m_shader.get();
    triItem.transform.SetTranslation({-1.2f, 0.0f, -3.0f});
    AddObject(triItem);

    RenderItem quadItem;
    quadItem.mesh   = m_quad.get();
    quadItem.shader = m_shader.get();
    quadItem.transform.SetTranslation({-0.4f, 0.0f, -3.0f});
    AddObject(quadItem);

    RenderItem staticCube;
    staticCube.mesh   = m_cube.get();
    staticCube.shader = m_shader.get();
    staticCube.transform.SetTranslation({0.4f, 0.0f, -3.0f});
    AddObject(staticCube);

    RenderItem playerCube;
    playerCube.mesh   = m_cube.get();
    playerCube.shader = m_shader.get();
    playerCube.transform.SetTranslation(m_playerPosition);
    m_playerCubeIdx = AddObject(playerCube);

    RenderItem sphereItem;
    sphereItem.mesh     = m_sphere.get();
    sphereItem.shader   = m_shader.get();
    sphereItem.drawMode = DrawMode::Wireframe;
    sphereItem.transform.SetTranslation({1.2f, 0.0f, -3.0f});
    AddObject(sphereItem);

    RenderItem smallCube;
    smallCube.mesh   = m_cube.get();
    smallCube.shader = m_shader.get();
    smallCube.transform.SetTranslation({-2.0f, 0.0f, -4.0f});
    smallCube.transform.SetScale({0.5f, 0.5f, 0.5f});
    AddObject(smallCube);

    RenderItem farCube;
    farCube.mesh   = m_cube.get();
    farCube.shader = m_shader.get();
    farCube.transform.SetTranslation({1.5f, 0.5f, -6.0f});
    farCube.transform.SetScale({0.7f, 0.7f, 0.7f});
    AddObject(farCube);

    spdlog::info("[SampleScene] Ready — triangle, quad, cubes, sphere");
    return true;
}

void SampleScene::OnUpdate(float deltaTime, KeyboardInput& input, MouseInput& mouse) {
    // TAB — toggle mouse capture
    if (input.IsKeyPressed(GLFW_KEY_TAB))
        mouse.SetCaptured(!mouse.IsCaptured());

    Camera& cam = GetCamera();

    // Camera mode switching
    if (input.IsKeyPressed(GLFW_KEY_F1)) {
        cam.SetMode(CameraMode::FreeFly);
        spdlog::info("[Camera] mode: FreeFly");
    }
    if (input.IsKeyPressed(GLFW_KEY_F2)) {
        cam.SetMode(CameraMode::FirstPerson);
        spdlog::info("[Camera] mode: FirstPerson");
    }
    if (input.IsKeyPressed(GLFW_KEY_F3)) {
        cam.SetMode(CameraMode::ThirdPerson);
        cam.SetOrbitTarget({0.0f, 0.0f, 0.0f});
        cam.SetOrbitRadius(5.0f);
        spdlog::info("[Camera] mode: ThirdPerson");
    }

    // Mouse look
    if (mouse.IsCaptured())
        cam.Rotate(mouse.GetDeltaX() * 0.1f, -mouse.GetDeltaY() * 0.1f);

    // Axis inputs
    constexpr float kSpeed = 3.0f;
    float fwd = 0.0f, right = 0.0f, up = 0.0f;
    if (input.IsKeyDown(GLFW_KEY_W))             fwd   += 1.0f;
    if (input.IsKeyDown(GLFW_KEY_S))             fwd   -= 1.0f;
    if (input.IsKeyDown(GLFW_KEY_D))             right += 1.0f;
    if (input.IsKeyDown(GLFW_KEY_A))             right -= 1.0f;
    if (input.IsKeyDown(GLFW_KEY_SPACE))         up    += 1.0f;
    if (input.IsKeyDown(GLFW_KEY_LEFT_CONTROL))  up    -= 1.0f;

    if (cam.GetMode() == CameraMode::FreeFly) {
        if (fwd   > 0.0f) cam.Move(CameraDirection::Forward,  kSpeed, deltaTime);
        if (fwd   < 0.0f) cam.Move(CameraDirection::Backward, kSpeed, deltaTime);
        if (right > 0.0f) cam.Move(CameraDirection::Right,    kSpeed, deltaTime);
        if (right < 0.0f) cam.Move(CameraDirection::Left,     kSpeed, deltaTime);
        if (up    > 0.0f) cam.Move(CameraDirection::Up,       kSpeed, deltaTime);
        if (up    < 0.0f) cam.Move(CameraDirection::Down,     kSpeed, deltaTime);
    } else if (cam.GetMode() == CameraMode::FirstPerson) {
        const glm::vec3 fwdXZ   = glm::normalize(glm::vec3(cam.GetForward().x, 0.0f, cam.GetForward().z));
        const glm::vec3 rightXZ = glm::normalize(glm::vec3(cam.GetRight().x,   0.0f, cam.GetRight().z));
        m_playerPosition += fwdXZ * (fwd * kSpeed * deltaTime)
                          + rightXZ * (right * kSpeed * deltaTime)
                          + glm::vec3(0.0f, up * kSpeed * deltaTime, 0.0f);
        cam.SetPosition(m_playerPosition + glm::vec3(0.0f, 1.7f, 0.0f));
    } else if (cam.GetMode() == CameraMode::ThirdPerson) {
        const glm::vec3 fwdXZ   = glm::normalize(glm::vec3(cam.GetForward().x, 0.0f, cam.GetForward().z));
        const glm::vec3 rightXZ = glm::normalize(glm::vec3(cam.GetRight().x,   0.0f, cam.GetRight().z));
        m_playerPosition += fwdXZ * (fwd * kSpeed * deltaTime)
                          + rightXZ * (right * kSpeed * deltaTime)
                          + glm::vec3(0.0f, up * kSpeed * deltaTime, 0.0f);
        cam.SetOrbitTarget(m_playerPosition);
    }

    // Keep player cube in sync
    GetObject(m_playerCubeIdx).transform.SetTranslation(m_playerPosition);

    // Debug log when moving or looking
    const bool moving   = fwd != 0.0f || right != 0.0f || up != 0.0f;
    const bool rotating = mouse.IsCaptured() && (mouse.GetDeltaX() != 0.0f || mouse.GetDeltaY() != 0.0f);
    if (moving || rotating) {
        const glm::vec3 pos = cam.GetPosition();
        spdlog::info("[Camera] pos=({:.2f}, {:.2f}, {:.2f})  yaw={:.1f}°  pitch={:.1f}°",
                     pos.x, pos.y, pos.z, cam.GetYaw(), cam.GetPitch());
    }
}
