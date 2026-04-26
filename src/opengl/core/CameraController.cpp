#include "core/CameraController.h"
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

CameraController::CameraController(Camera& camera)
    : m_camera(camera)
{
}

FreeFlyController::FreeFlyController(Camera& camera)
    : CameraController(camera)
{
}

void FreeFlyController::Update(float deltaTime, const IInputProvider& input) {
    if (input.IsMouseCaptured()) {
        m_camera.Rotate(input.GetMouseDeltaX() * m_mouseSensitivity,
                        -input.GetMouseDeltaY() * m_mouseSensitivity);
    }

    if (input.IsKeyDown(GLFW_KEY_W)) m_camera.Move(CameraDirection::Forward,  m_moveSpeed, deltaTime);
    if (input.IsKeyDown(GLFW_KEY_S)) m_camera.Move(CameraDirection::Backward, m_moveSpeed, deltaTime);
    if (input.IsKeyDown(GLFW_KEY_A)) m_camera.Move(CameraDirection::Left,     m_moveSpeed, deltaTime);
    if (input.IsKeyDown(GLFW_KEY_D)) m_camera.Move(CameraDirection::Right,    m_moveSpeed, deltaTime);
    if (input.IsKeyDown(GLFW_KEY_SPACE)) m_camera.Move(CameraDirection::Up,   m_moveSpeed, deltaTime);
    if (input.IsKeyDown(GLFW_KEY_LEFT_CONTROL)) m_camera.Move(CameraDirection::Down, m_moveSpeed, deltaTime);
}

FirstPersonController::FirstPersonController(Camera& camera)
    : CameraController(camera)
{
    m_camera.SetMode(CameraMode::FirstPerson);
}

void FirstPersonController::Update(float deltaTime, const IInputProvider& input) {
    if (input.IsMouseCaptured()) {
        m_camera.Rotate(input.GetMouseDeltaX() * m_mouseSensitivity,
                        -input.GetMouseDeltaY() * m_mouseSensitivity);
    }

    if (input.IsKeyDown(GLFW_KEY_W)) m_camera.Move(CameraDirection::Forward,  m_moveSpeed, deltaTime);
    if (input.IsKeyDown(GLFW_KEY_S)) m_camera.Move(CameraDirection::Backward, m_moveSpeed, deltaTime);
    if (input.IsKeyDown(GLFW_KEY_A)) m_camera.Move(CameraDirection::Left,     m_moveSpeed, deltaTime);
    if (input.IsKeyDown(GLFW_KEY_D)) m_camera.Move(CameraDirection::Right,    m_moveSpeed, deltaTime);
    if (input.IsKeyDown(GLFW_KEY_SPACE)) m_camera.Move(CameraDirection::Up,   m_moveSpeed, deltaTime);
    if (input.IsKeyDown(GLFW_KEY_LEFT_CONTROL)) m_camera.Move(CameraDirection::Down, m_moveSpeed, deltaTime);
}

StandardSceneCameraController::StandardSceneCameraController(Camera& camera)
    : m_camera(camera)
{
}

void StandardSceneCameraController::Update(float deltaTime,
                                           IInputProvider& input,
                                           glm::vec3& playerPos,
                                           glm::vec3& outMoveDirXZ,
                                           float orbitTargetYOffset,
                                           float& cameraFreeFlySpeed,
                                           float& cameraFirstPersonSpeed,
                                           float& cameraThirdPersonSpeed,
                                           float& cameraOrbitRadius,
                                           float& outLastEffectiveSpeed) {
    // TAB is a persistent toggle; RMB is temporary hold-to-look.
    const bool rmbDown = input.IsMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT);

    if (input.IsKeyPressed(GLFW_KEY_TAB)) {
        if (m_rmbHoldActive) {
            // While RMB-holding, toggle the baseline state restored on release.
            m_capturedBeforeRmbHold = !m_capturedBeforeRmbHold;
        } else {
            input.SetMouseCaptured(!input.IsMouseCaptured());
        }
    }

    if (rmbDown && !m_rmbHoldActive) {
        m_rmbHoldActive = true;
        m_capturedBeforeRmbHold = input.IsMouseCaptured();
        if (!input.IsMouseCaptured()) {
            input.SetMouseCaptured(true);
        }
    } else if (!rmbDown && m_rmbHoldActive) {
        m_rmbHoldActive = false;
        if (!m_capturedBeforeRmbHold) {
            input.SetMouseCaptured(false);
        }
    }

    // Camera mode switching.
    if (input.IsKeyPressed(GLFW_KEY_F1)) {
        m_camera.SetMode(CameraMode::FreeFly);
        spdlog::debug("[Camera] mode: FreeFly");
    }
    if (input.IsKeyPressed(GLFW_KEY_F2)) {
        m_camera.SetMode(CameraMode::FirstPerson);
        spdlog::debug("[Camera] mode: FirstPerson");
    }
    if (input.IsKeyPressed(GLFW_KEY_F3)) {
        m_camera.SetMode(CameraMode::ThirdPerson);
        m_camera.SetOrbitTarget(playerPos);
        m_camera.SetOrbitRadius(cameraOrbitRadius);
        spdlog::debug("[Camera] mode: ThirdPerson");
    }

    if (input.IsMouseCaptured())
        m_camera.Rotate(input.GetMouseDeltaX() * m_mouseSensitivity, -input.GetMouseDeltaY() * m_mouseSensitivity);

    float currentFreeFly = cameraFreeFlySpeed;
    float currentFirstPerson = cameraFirstPersonSpeed;
    float currentThirdPerson = cameraThirdPersonSpeed;

    const float scrollDelta = input.GetMouseScrollDeltaY();
    if (scrollDelta != 0.0f) {
        if (m_camera.GetMode() == CameraMode::ThirdPerson) {
            constexpr float kOrbitZoomStep = 0.75f;
            constexpr float kOrbitZoomMin = 1.0f;
            constexpr float kOrbitZoomMax = 30.0f;

            cameraOrbitRadius -= scrollDelta * kOrbitZoomStep;
            if (cameraOrbitRadius < kOrbitZoomMin) cameraOrbitRadius = kOrbitZoomMin;
            if (cameraOrbitRadius > kOrbitZoomMax) cameraOrbitRadius = kOrbitZoomMax;

            m_camera.SetOrbitRadius(cameraOrbitRadius);
            spdlog::debug("[Camera] ThirdPerson zoom radius: {:.2f}", cameraOrbitRadius);
        } else if (m_camera.GetMode() == CameraMode::FreeFly || m_camera.GetMode() == CameraMode::FirstPerson) {
            constexpr float kFovZoomStep = 2.0f;
            const float newFov = m_camera.GetFOV() - scrollDelta * kFovZoomStep;
            m_camera.SetFOV(newFov);
            spdlog::debug("[Camera] {} zoom FOV: {:.1f}",
                         m_camera.GetMode() == CameraMode::FirstPerson ? "FirstPerson" : "FreeFly",
                         m_camera.GetFOV());
        }
    }

    const float sprintMultiplier =
        (input.IsKeyDown(GLFW_KEY_LEFT_SHIFT) || input.IsKeyDown(GLFW_KEY_RIGHT_SHIFT)) ? m_sprintMultiplier : 1.0f;
    currentFreeFly *= sprintMultiplier;
    currentFirstPerson *= sprintMultiplier;
    currentThirdPerson *= sprintMultiplier;

    float fwd = 0.0f, right = 0.0f, up = 0.0f;
    if (input.IsKeyDown(GLFW_KEY_W))            fwd   += 1.0f;
    if (input.IsKeyDown(GLFW_KEY_S))            fwd   -= 1.0f;
    if (input.IsKeyDown(GLFW_KEY_D))            right += 1.0f;
    if (input.IsKeyDown(GLFW_KEY_A))            right -= 1.0f;
    if (input.IsKeyDown(GLFW_KEY_SPACE))        up    += 1.0f;
    if (input.IsKeyDown(GLFW_KEY_LEFT_CONTROL)) up    -= 1.0f;

    outMoveDirXZ = glm::vec3(0.0f);

    if (m_camera.GetMode() == CameraMode::FreeFly) {
        outLastEffectiveSpeed = currentFreeFly;
        if (fwd   > 0.0f) m_camera.Move(CameraDirection::Forward,  currentFreeFly, deltaTime);
        if (fwd   < 0.0f) m_camera.Move(CameraDirection::Backward, currentFreeFly, deltaTime);
        if (right > 0.0f) m_camera.Move(CameraDirection::Right,    currentFreeFly, deltaTime);
        if (right < 0.0f) m_camera.Move(CameraDirection::Left,     currentFreeFly, deltaTime);
        if (up    > 0.0f) m_camera.Move(CameraDirection::Up,       currentFreeFly, deltaTime);
        if (up    < 0.0f) m_camera.Move(CameraDirection::Down,     currentFreeFly, deltaTime);
    } else {
        const glm::vec3 fwdXZ   = glm::normalize(glm::vec3(m_camera.GetForward().x, 0.0f, m_camera.GetForward().z));
        const glm::vec3 rightXZ = glm::normalize(glm::vec3(m_camera.GetRight().x,   0.0f, m_camera.GetRight().z));
        outMoveDirXZ = fwdXZ * fwd + rightXZ * right;

        if (m_camera.GetMode() == CameraMode::FirstPerson) {
            outLastEffectiveSpeed = currentFirstPerson;
            playerPos += outMoveDirXZ * (currentFirstPerson * deltaTime);
            playerPos.y += up * currentFirstPerson * deltaTime;
            m_camera.SetPosition(playerPos);
        } else if (m_camera.GetMode() == CameraMode::ThirdPerson) {
            outLastEffectiveSpeed = currentThirdPerson;
            playerPos += outMoveDirXZ * (currentThirdPerson * deltaTime);
            playerPos.y += up * currentThirdPerson * deltaTime;
            m_camera.SetOrbitTarget(playerPos + glm::vec3(0.0f, orbitTargetYOffset, 0.0f));
        }
    }

    const bool moving   = fwd != 0.0f || right != 0.0f || up != 0.0f;
    const bool rotating = input.IsMouseCaptured() && (input.GetMouseDeltaX() != 0.0f || input.GetMouseDeltaY() != 0.0f);
    if (moving || rotating) {
        const glm::vec3 pos = m_camera.GetPosition();
        spdlog::debug("[Camera] pos=({:.2f}, {:.2f}, {:.2f})  yaw={:.1f}deg  pitch={:.1f}deg",
                     pos.x, pos.y, pos.z, m_camera.GetYaw(), m_camera.GetPitch());
    }
}
