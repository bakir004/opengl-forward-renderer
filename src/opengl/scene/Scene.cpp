#include "scene/Scene.h"
#include "scene/FrameSubmission.h"
#include "core/KeyboardInput.h"
#include "core/MouseInput.h"
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

void Scene::SetCamera(Camera camera) {
    m_camera = std::move(camera);
}

void Scene::SetClearColor(glm::vec4 color) {
    m_clearColor = color;
}

size_t Scene::AddObject(RenderItem item) {
    if (!item.material && !item.shader)
        spdlog::warn("[Scene] AddObject: RenderItem has no material or shader — will render with error shader");
    m_objects.push_back(std::move(item));
    return m_objects.size() - 1;
}

RenderItem& Scene::GetObject(size_t index) {
    return m_objects[index];
}

Camera& Scene::GetCamera() {
    return m_camera;
}

float Scene::GetCurrentCameraSpeed() const {
    return m_lastEffectiveSpeed;
}

void Scene::InternalUpdate(float deltaTime, KeyboardInput& input, MouseInput& mouse, int fbWidth, int fbHeight) {
    m_camera.OnResize(fbWidth, fbHeight);
    OnUpdate(deltaTime, input, mouse);
}

void Scene::BuildSubmission(FrameSubmission& out) const {
    out.camera                   = &m_camera;
    out.clearInfo.clearColor     = m_clearColor;
    out.clearInfo.clearFlags     = ClearFlags::ColorDepth;
    out.objects                  = m_objects;
}

void Scene::UpdateStandardCameraAndPlayer(float deltaTime, KeyboardInput& input, MouseInput& mouse, 
                                          glm::vec3& playerPos, glm::vec3& outMoveDirXZ, 
                                          float orbitTargetYOffset) 
{
    // TAB — toggle mouse capture
    if (input.IsKeyPressed(GLFW_KEY_TAB))
        mouse.SetCaptured(!mouse.IsCaptured());

    Camera& cam = GetCamera();

    // Camera mode switching
    if (input.IsKeyPressed(GLFW_KEY_F1)) {
        cam.SetMode(CameraMode::FreeFly);
        spdlog::debug("[Camera] mode: FreeFly");
    }
    if (input.IsKeyPressed(GLFW_KEY_F2)) {
        cam.SetMode(CameraMode::FirstPerson);
        spdlog::debug("[Camera] mode: FirstPerson");
    }
    if (input.IsKeyPressed(GLFW_KEY_F3)) {
        cam.SetMode(CameraMode::ThirdPerson);
        cam.SetOrbitTarget(playerPos);
        cam.SetOrbitRadius(m_cameraOrbitRadius);
        spdlog::debug("[Camera] mode: ThirdPerson");
    }

    // Mouse look
    if (mouse.IsCaptured())
        cam.Rotate(mouse.GetDeltaX() * 0.1f, -mouse.GetDeltaY() * 0.1f);

    float currentFreeFly = m_cameraFreeFlySpeed;
    float currentFirstPerson = m_cameraFirstPersonSpeed;
    float currentThirdPerson = m_cameraThirdPersonSpeed;

    // Scroll wheel speed adjustment ONLY for ThirdPerson camera
    if (cam.GetMode() == CameraMode::ThirdPerson) {
        float scrollDelta = mouse.GetScrollDeltaY();
        if (scrollDelta != 0.0f) {
            m_cameraThirdPersonSpeed += scrollDelta * 0.5f; // sensitivity
            if (m_cameraThirdPersonSpeed < 0.1f) m_cameraThirdPersonSpeed = 0.1f;
            if (m_cameraThirdPersonSpeed > 100.0f) m_cameraThirdPersonSpeed = 100.0f;
            spdlog::debug("[Camera] ThirdPerson Speed updated to: {:.2f}", m_cameraThirdPersonSpeed);
        }
        
        currentThirdPerson = m_cameraThirdPersonSpeed;
        if (input.IsKeyDown(GLFW_KEY_LEFT_SHIFT) || input.IsKeyDown(GLFW_KEY_RIGHT_SHIFT)) {
            currentThirdPerson *= 3.0f; // Sprint multiplier
        }
    }

    // Axis inputs
    float fwd = 0.0f, right = 0.0f, up = 0.0f;
    if (input.IsKeyDown(GLFW_KEY_W))            fwd   += 1.0f;
    if (input.IsKeyDown(GLFW_KEY_S))            fwd   -= 1.0f;
    if (input.IsKeyDown(GLFW_KEY_D))            right += 1.0f;
    if (input.IsKeyDown(GLFW_KEY_A))            right -= 1.0f;
    if (input.IsKeyDown(GLFW_KEY_SPACE))        up    += 1.0f;
    if (input.IsKeyDown(GLFW_KEY_LEFT_CONTROL)) up    -= 1.0f;

    outMoveDirXZ = glm::vec3(0.0f);

    if (cam.GetMode() == CameraMode::FreeFly) {
        m_lastEffectiveSpeed = currentFreeFly;
        if (fwd   > 0.0f) cam.Move(CameraDirection::Forward,  currentFreeFly, deltaTime);
        if (fwd   < 0.0f) cam.Move(CameraDirection::Backward, currentFreeFly, deltaTime);
        if (right > 0.0f) cam.Move(CameraDirection::Right,    currentFreeFly, deltaTime);
        if (right < 0.0f) cam.Move(CameraDirection::Left,     currentFreeFly, deltaTime);
        if (up    > 0.0f) cam.Move(CameraDirection::Up,       currentFreeFly, deltaTime);
        if (up    < 0.0f) cam.Move(CameraDirection::Down,     currentFreeFly, deltaTime);
    } else {
        const glm::vec3 fwdXZ   = glm::normalize(glm::vec3(cam.GetForward().x, 0.0f, cam.GetForward().z));
        const glm::vec3 rightXZ = glm::normalize(glm::vec3(cam.GetRight().x,   0.0f, cam.GetRight().z));
        outMoveDirXZ = fwdXZ * fwd + rightXZ * right;
        
        if (cam.GetMode() == CameraMode::FirstPerson) {
            m_lastEffectiveSpeed = currentFirstPerson;
            playerPos += outMoveDirXZ * (currentFirstPerson * deltaTime);
            playerPos.y += up * currentFirstPerson * deltaTime;
            cam.SetPosition(playerPos);
        } else if (cam.GetMode() == CameraMode::ThirdPerson) {
            m_lastEffectiveSpeed = currentThirdPerson;
            playerPos += outMoveDirXZ * (currentThirdPerson * deltaTime);
            cam.SetOrbitTarget(playerPos + glm::vec3(0.0f, orbitTargetYOffset, 0.0f));
        }
    }

    // Debug log when moving or looking
    const bool moving   = fwd != 0.0f || right != 0.0f || up != 0.0f;
    const bool rotating = mouse.IsCaptured() && (mouse.GetDeltaX() != 0.0f || mouse.GetDeltaY() != 0.0f);
    if (moving || rotating) {
        const glm::vec3 pos = cam.GetPosition();
        spdlog::debug("[Camera] pos=({:.2f}, {:.2f}, {:.2f})  yaw={:.1f}°  pitch={:.1f}°",
                     pos.x, pos.y, pos.z, cam.GetYaw(), cam.GetPitch());
    }
}
