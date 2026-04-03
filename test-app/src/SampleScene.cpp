#include "SampleScene.h"

#include "core/Camera.h"
#include "core/KeyboardInput.h"
#include "core/MouseInput.h"
#include "core/Primitives.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

bool SampleScene::Setup() {
    spdlog::info("[SampleScene] Setting up");

    m_shader = std::make_unique<ShaderProgram>("assets/shaders/basic.vert", "assets/shaders/basic.frag");
    if (!m_shader->IsValid()) {
        spdlog::error("[SampleScene] Shader failed to compile/link — aborting setup");
        return false;
    }

    // ── Geometry ─────────────────────────────────────────────────────────────

    // Rainbow primitives — default ColorMode::Rainbow
    m_triangle   = std::make_unique<MeshBuffer>(GenerateTriangle().CreateMeshBuffer());
    m_quad       = std::make_unique<MeshBuffer>(GenerateQuad().CreateMeshBuffer());
    m_rainbowCube= std::make_unique<MeshBuffer>(GenerateCube().CreateMeshBuffer());

    // Solid-coloured cube — steel blue, faces slightly varied so edges read in 3-D
    m_solidCube  = std::make_unique<MeshBuffer>(
        GenerateCube({ .colorMode = ColorMode::Solid,
                       .baseColor = {0.30f, 0.55f, 0.90f},
                       .doubleSided = true }).CreateMeshBuffer());

    // Pyramid — solid warm orange
    m_pyramid    = std::make_unique<MeshBuffer>(
        GeneratePyramid({ .colorMode = ColorMode::Solid,
                          .baseColor = {1.0f, 0.45f, 0.10f} }).CreateMeshBuffer());

    // Sphere — double-sided so wireframe rendering shows all edges (no half-sphere artifact)
    m_sphere     = std::make_unique<MeshBuffer>(
        GenerateSphere(0.4f, 16, { .doubleSided = true }).CreateMeshBuffer());

    // ── Camera ───────────────────────────────────────────────────────────────
    Camera cam;
    cam.SetPosition({0.0f, 1.0f, 5.0f});
    SetCamera(cam);
    SetClearColor({0.08f, 0.09f, 0.12f, 1.0f});

    // ── Objects ───────────────────────────────────────────────────────────────
    //  Layout (all at z = -3 unless noted):
    //
    //   -2.0   -1.2   -0.4    0.4    1.2    2.0   x
    //      tri   quad  solidC  rbwC  sphere
    //                  pyra (z=-4.5, between rows)
    //   smallCube (z=-5) / farCube (z=-7)

    RenderItem triItem;
    triItem.mesh   = m_triangle.get();
    triItem.shader = m_shader.get();
    triItem.transform.SetTranslation({-1.2f, 0.0f, -3.0f});
    triItem.transform.SetScale({0.6f, 0.6f, 0.6f});
    AddObject(triItem);

    RenderItem quadItem;
    quadItem.mesh   = m_quad.get();
    quadItem.shader = m_shader.get();
    quadItem.transform.SetTranslation({-0.4f, 0.0f, -3.0f});
    quadItem.transform.SetScale({0.6f, 0.6f, 0.6f});
    AddObject(quadItem);

    // Solid blue cube (static)
    RenderItem solidCubeItem;
    solidCubeItem.mesh   = m_solidCube.get();
    solidCubeItem.shader = m_shader.get();
    solidCubeItem.transform.SetTranslation({0.4f, 0.0f, -3.0f});
    solidCubeItem.transform.SetScale({0.6f, 0.6f, 0.6f});
    AddObject(solidCubeItem);

    // Rainbow cube — player-controlled
    RenderItem playerCube;
    playerCube.mesh   = m_rainbowCube.get();
    playerCube.shader = m_shader.get();
    playerCube.transform.SetTranslation(m_playerPosition);
    playerCube.transform.SetScale({0.7f, 0.7f, 0.7f});
    m_playerCubeIdx = AddObject(playerCube);

    // Sphere — wireframe, double-sided
    RenderItem sphereItem;
    sphereItem.mesh     = m_sphere.get();
    sphereItem.shader   = m_shader.get();
    sphereItem.drawMode = DrawMode::Wireframe;
    sphereItem.transform.SetTranslation({1.2f, 0.0f, -3.0f});
    AddObject(sphereItem);

    // Pyramid — solid orange, slightly back and below eye level
    RenderItem pyramidItem;
    pyramidItem.mesh   = m_pyramid.get();
    pyramidItem.shader = m_shader.get();
    pyramidItem.transform.SetTranslation({0.0f, -0.5f, -4.5f});
    pyramidItem.transform.SetScale({0.8f, 0.8f, 0.8f});
    AddObject(pyramidItem);

    // Small rainbow cube off to the side
    RenderItem smallCube;
    smallCube.mesh   = m_rainbowCube.get();
    smallCube.shader = m_shader.get();
    smallCube.transform.SetTranslation({-2.0f, 0.0f, -5.0f});
    smallCube.transform.SetScale({0.5f, 0.5f, 0.5f});
    AddObject(smallCube);

    // Distant solid cube
    RenderItem farCube;
    farCube.mesh   = m_solidCube.get();
    farCube.shader = m_shader.get();
    farCube.transform.SetTranslation({1.5f, 0.5f, -7.0f});
    farCube.transform.SetScale({0.7f, 0.7f, 0.7f});
    AddObject(farCube);

    spdlog::info("[SampleScene] Ready — triangle | quad | solid-cube | rainbow-cube | "
                 "sphere (wireframe) | pyramid | + 2 background cubes");
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
    constexpr float freeFlySpeed = 4.0f;
    constexpr float kSpeed = 3.0f;
    float fwd = 0.0f, right = 0.0f, up = 0.0f;
    if (input.IsKeyDown(GLFW_KEY_W))            fwd   += 1.0f;
    if (input.IsKeyDown(GLFW_KEY_S))            fwd   -= 1.0f;
    if (input.IsKeyDown(GLFW_KEY_D))            right += 1.0f;
    if (input.IsKeyDown(GLFW_KEY_A))            right -= 1.0f;
    if (input.IsKeyDown(GLFW_KEY_SPACE))        up    += 1.0f;
    if (input.IsKeyDown(GLFW_KEY_LEFT_CONTROL)) up    -= 1.0f;

    glm::vec3 moveDirXZ(0.0f);  // horizontal movement direction this frame

    if (cam.GetMode() == CameraMode::FreeFly) {
        if (fwd   > 0.0f) cam.Move(CameraDirection::Forward,  freeFlySpeed, deltaTime);
        if (fwd   < 0.0f) cam.Move(CameraDirection::Backward, freeFlySpeed, deltaTime);
        if (right > 0.0f) cam.Move(CameraDirection::Right,    freeFlySpeed, deltaTime);
        if (right < 0.0f) cam.Move(CameraDirection::Left,     freeFlySpeed, deltaTime);
        if (up    > 0.0f) cam.Move(CameraDirection::Up,       freeFlySpeed, deltaTime);
        if (up    < 0.0f) cam.Move(CameraDirection::Down,     freeFlySpeed, deltaTime);
    } else if (cam.GetMode() == CameraMode::FirstPerson) {
        const glm::vec3 fwdXZ   = glm::normalize(glm::vec3(cam.GetForward().x, 0.0f, cam.GetForward().z));
        const glm::vec3 rightXZ = glm::normalize(glm::vec3(cam.GetRight().x,   0.0f, cam.GetRight().z));
        moveDirXZ = fwdXZ * fwd + rightXZ * right;
        m_playerPosition += moveDirXZ * (kSpeed * deltaTime);
        cam.SetPosition(m_playerPosition + glm::vec3(0.0f, 1.7f, 0.0f));
    } else if (cam.GetMode() == CameraMode::ThirdPerson) {
        const glm::vec3 fwdXZ   = glm::normalize(glm::vec3(cam.GetForward().x, 0.0f, cam.GetForward().z));
        const glm::vec3 rightXZ = glm::normalize(glm::vec3(cam.GetRight().x,   0.0f, cam.GetRight().z));
        moveDirXZ = fwdXZ * fwd + rightXZ * right;
        m_playerPosition += moveDirXZ * (kSpeed * deltaTime);
        cam.SetOrbitTarget(m_playerPosition);
    }

    // Keep player cube in sync
    auto& playerTransform = GetObject(m_playerCubeIdx).transform;
    playerTransform.SetTranslation(m_playerPosition);

    // Rotate to face the direction of movement (XZ plane only)
    if (glm::length(moveDirXZ) > 0.001f) {
        const glm::vec3 d = glm::normalize(moveDirXZ);
        const float yaw = glm::degrees(std::atan2(d.x, d.z));
        playerTransform.SetRotationEulerDegrees({0.0f, yaw, 0.0f});
    }

    // Debug log when moving or looking
    const bool moving   = fwd != 0.0f || right != 0.0f || up != 0.0f;
    const bool rotating = mouse.IsCaptured() && (mouse.GetDeltaX() != 0.0f || mouse.GetDeltaY() != 0.0f);
    if (moving || rotating) {
        const glm::vec3 pos = cam.GetPosition();
        spdlog::info("[Camera] pos=({:.2f}, {:.2f}, {:.2f})  yaw={:.1f}°  pitch={:.1f}°",
                     pos.x, pos.y, pos.z, cam.GetYaw(), cam.GetPitch());
    }
}
