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
    // e) a camera starting position that immediately shows depth and perspective
    cam.SetPosition({0.0f, 3.0f, 8.0f});
    cam.SetOrientation(-90.0f, -15.0f); // Look down slightly
    SetCamera(cam);
    SetClearColor({0.08f, 0.09f, 0.12f, 1.0f});

    // ── Objects ───────────────────────────────────────────────────────────────
    
    // d) a ground plane
    RenderItem groundItem;
    groundItem.mesh   = m_quad.get();
    groundItem.shader = m_shader.get();
    groundItem.transform.SetTranslation({0.0f, -1.0f, 0.0f});
    groundItem.transform.SetRotationEulerDegrees({-90.0f, 0.0f, 0.0f});
    groundItem.transform.SetScale({20.0f, 20.0f, 20.0f});
    AddObject(groundItem);

    // a), b), c) several cubes and quads placed at different positions, scales, and rotations
    // Row 1: Cubes
    for (int i = 0; i < 5; ++i) {
        RenderItem cubeItem;
        cubeItem.mesh   = (i % 2 == 0) ? m_rainbowCube.get() : m_solidCube.get();
        cubeItem.shader = m_shader.get();
        
        float xPath = -4.0f + (i * 2.0f);
        cubeItem.transform.SetTranslation({xPath, 0.0f, -2.0f});
        
        float s = 0.5f + (i * 0.15f);
        cubeItem.transform.SetScale({s, s, s});
        
        float rotY = i * 25.0f; 
        float rotX = i * 15.0f;
        cubeItem.transform.SetRotationEulerDegrees({rotX, rotY, 0.0f});
        AddObject(cubeItem);
    }
    
    // Row 2: Quads (visualise transforms)
    for (int i = 0; i < 5; ++i) {
        RenderItem quadItem;
        quadItem.mesh   = m_quad.get();
        quadItem.shader = m_shader.get();
        
        float xPath = -4.0f + (i * 2.0f);
        quadItem.transform.SetTranslation({xPath, 0.5f, -5.0f});
        
        float sx = 0.6f + (i * 0.2f);
        float sy = 0.6f + ((4 - i) * 0.1f);
        quadItem.transform.SetScale({sx, sy, 1.0f});
        
        float rotZ = i * -15.0f; 
        quadItem.transform.SetRotationEulerDegrees({0.0f, 0.0f, rotZ});
        AddObject(quadItem);
    }

    // Row 3: Mixed extra shapes
    RenderItem pyItem;
    pyItem.mesh   = m_pyramid.get();
    pyItem.shader = m_shader.get();
    pyItem.transform.SetTranslation({-2.0f, 1.5f, -8.0f});
    pyItem.transform.SetScale({1.5f, 1.5f, 1.5f});
    m_pyramidIdx = AddObject(pyItem);

    RenderItem sphItem;
    sphItem.mesh     = m_sphere.get();
    sphItem.shader   = m_shader.get();
    sphItem.drawMode = DrawMode::Wireframe;
    sphItem.transform.SetTranslation({2.0f, 1.5f, -8.0f});
    sphItem.transform.SetScale({2.0f, 2.0f, 2.0f});
    AddObject(sphItem);

    // Player Cube (so OnUpdate has a valid index)
    RenderItem playerCube;
    playerCube.mesh   = m_rainbowCube.get();
    playerCube.shader = m_shader.get();
    playerCube.transform.SetTranslation(m_playerPosition);
    playerCube.transform.SetScale({0.7f, 0.7f, 0.7f});
    m_playerCubeIdx = AddObject(playerCube);

    spdlog::info("[SampleScene] Ready — detailed test scene with ground, rows of cubes/quads loaded");
    return true;
}

void SampleScene::OnUpdate(float deltaTime, KeyboardInput& input, MouseInput& mouse) {
    // TAB — toggle mouse capture
    if (input.IsKeyPressed(GLFW_KEY_TAB))
        mouse.SetCaptured(!mouse.IsCaptured());

    // Animate pyramid continuously
    m_pyramidRotY += 60.0f * deltaTime; // 60 degrees per sec
    auto& pyTransform = GetObject(m_pyramidIdx).transform;
    pyTransform.SetRotationEulerDegrees({0.0f, m_pyramidRotY, 0.0f});

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
        cam.SetOrbitTarget(m_playerPosition);
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
