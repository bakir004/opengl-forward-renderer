#include "core/InputManager.h"
#include "core/KeyboardInput.h"
#include "core/MouseInput.h"
#include <imgui.h>
#include <GLFW/glfw3.h>

InputManager::InputManager(GLFWwindow* window)
    : m_keyboard(std::make_unique<KeyboardInput>(window))
    , m_mouse(std::make_unique<MouseInput>(window))
{
}

InputManager::~InputManager() = default;

void InputManager::Update() {
    m_keyboard->Update();
    m_mouse->Update();
}

bool InputManager::IsKeyDown(int keyCode) const {
    if (ShouldBlockKeyboard()) return false;
    return m_keyboard->IsKeyDown(keyCode);
}

bool InputManager::IsKeyPressed(int keyCode) const {
    if (ShouldBlockKeyboard()) return false;
    return m_keyboard->IsKeyPressed(keyCode);
}

bool InputManager::IsMouseButtonDown(int button) const {
    if (ShouldBlockMouse()) {
        // Allow RMB to pass through even if ImGui wants the mouse, 
        // so we can use it to trigger camera look.
        if (button != GLFW_MOUSE_BUTTON_RIGHT) {
            return false;
        }
    }
    return m_mouse->IsButtonDown(button);
}

bool InputManager::IsMouseButtonPressed(int button) const {
    if (ShouldBlockMouse()) {
        if (button != GLFW_MOUSE_BUTTON_RIGHT) {
            return false;
        }
    }
    return m_mouse->IsButtonPressed(button);
}

float InputManager::GetMouseDeltaX() const {
    if (ShouldBlockMouse()) return 0.0f;
    return m_mouse->GetDeltaX();
}

float InputManager::GetMouseDeltaY() const {
    if (ShouldBlockMouse()) return 0.0f;
    return m_mouse->GetDeltaY();
}

float InputManager::GetMouseScrollDeltaY() const {
    if (ShouldBlockMouse()) return 0.0f;
    return m_mouse->GetScrollDeltaY();
}

bool InputManager::IsMouseCaptured() const {
    return m_mouse->IsCaptured();
}

void InputManager::SetMouseCaptured(bool captured) {
    m_mouse->SetCaptured(captured);
}

bool InputManager::ShouldBlockKeyboard() const {
    ImGuiIO& io = ImGui::GetIO();
    return io.WantCaptureKeyboard;
}

bool InputManager::ShouldBlockMouse() const {
    // If mouse is captured (FPS mode), we should NOT block it even if ImGui wants it,
    // because the user is actively controlling the camera.
    // However, usually ImGui won't want capture if the cursor is disabled.
    if (m_mouse->IsCaptured()) return false;

    ImGuiIO& io = ImGui::GetIO();
    return io.WantCaptureMouse;
}
