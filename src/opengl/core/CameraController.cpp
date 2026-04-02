#include "core/CameraController.h"
#include <GLFW/glfw3.h>

CameraController::CameraController(Camera& camera)
    : m_camera(camera)
{
}

FreeFlyController::FreeFlyController(Camera& camera)
    : CameraController(camera)
{
}

void FreeFlyController::Update(float deltaTime, const KeyboardInput& keyboard, const MouseInput& mouse) {
    if (mouse.IsCaptured()) {
        m_camera.Rotate(mouse.GetDeltaX() * m_mouseSensitivity,
                        -mouse.GetDeltaY() * m_mouseSensitivity);
    }

    if (keyboard.IsKeyDown(GLFW_KEY_W)) m_camera.Move(CameraDirection::Forward,  m_moveSpeed, deltaTime);
    if (keyboard.IsKeyDown(GLFW_KEY_S)) m_camera.Move(CameraDirection::Backward, m_moveSpeed, deltaTime);
    if (keyboard.IsKeyDown(GLFW_KEY_A)) m_camera.Move(CameraDirection::Left,     m_moveSpeed, deltaTime);
    if (keyboard.IsKeyDown(GLFW_KEY_D)) m_camera.Move(CameraDirection::Right,    m_moveSpeed, deltaTime);
    if (keyboard.IsKeyDown(GLFW_KEY_SPACE)) m_camera.Move(CameraDirection::Up,   m_moveSpeed, deltaTime);
    if (keyboard.IsKeyDown(GLFW_KEY_LEFT_CONTROL)) m_camera.Move(CameraDirection::Down, m_moveSpeed, deltaTime);
}

FirstPersonController::FirstPersonController(Camera& camera)
    : CameraController(camera)
{
    m_camera.SetMode(CameraMode::FirstPerson);
}

void FirstPersonController::Update(float deltaTime, const KeyboardInput& keyboard, const MouseInput& mouse) {
    if (mouse.IsCaptured()) {
        m_camera.Rotate(mouse.GetDeltaX() * m_mouseSensitivity,
                        -mouse.GetDeltaY() * m_mouseSensitivity);
    }

    if (keyboard.IsKeyDown(GLFW_KEY_W)) m_camera.Move(CameraDirection::Forward,  m_moveSpeed, deltaTime);
    if (keyboard.IsKeyDown(GLFW_KEY_S)) m_camera.Move(CameraDirection::Backward, m_moveSpeed, deltaTime);
    if (keyboard.IsKeyDown(GLFW_KEY_A)) m_camera.Move(CameraDirection::Left,     m_moveSpeed, deltaTime);
    if (keyboard.IsKeyDown(GLFW_KEY_D)) m_camera.Move(CameraDirection::Right,    m_moveSpeed, deltaTime);
    if (keyboard.IsKeyDown(GLFW_KEY_SPACE)) m_camera.Move(CameraDirection::Up,   m_moveSpeed, deltaTime);
    if (keyboard.IsKeyDown(GLFW_KEY_LEFT_CONTROL)) m_camera.Move(CameraDirection::Down, m_moveSpeed, deltaTime);
}
