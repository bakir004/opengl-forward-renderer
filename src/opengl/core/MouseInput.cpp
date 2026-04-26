#include "core/MouseInput.h"
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

MouseInput::MouseInput(GLFWwindow* window) : m_window(window) {}

void MouseInput::Update() {
    for (int i = 0; i < kButtonCount; ++i) {
        m_previousButtons[i] = m_currentButtons[i];
        m_currentButtons[i] = (glfwGetMouseButton(m_window, i) == GLFW_PRESS);
    }

    if (!m_captured) {
        m_deltaX = 0.0f;
        m_deltaY = 0.0f;
        m_deltaScrollY = m_accumScrollY;
        m_accumScrollY = 0.0f;
        return;
    }

    double x, y;
    glfwGetCursorPos(m_window, &x, &y);

    if (m_skipNext) {
        // First frame after capture: record position, emit no delta.
        // Without this, the delta would be the distance from wherever the cursor
        // last was to its current position — potentially huge.
        m_lastX    = x;
        m_lastY    = y;
        m_deltaX   = 0.0f;
        m_deltaY   = 0.0f;
        m_deltaScrollY = m_accumScrollY;
        m_accumScrollY = 0.0f;
        m_skipNext = false;
        return;
    }

    m_deltaX = static_cast<float>(x - m_lastX);
    m_deltaY = static_cast<float>(y - m_lastY);
    m_deltaScrollY = m_accumScrollY;
    m_accumScrollY = 0.0f;
    m_lastX  = x;
    m_lastY  = y;
}

void MouseInput::OnScroll(float yoffset) {
    m_accumScrollY += yoffset;
}

bool MouseInput::IsButtonDown(int glfwButton) const {
    if (glfwButton < 0 || glfwButton >= kButtonCount) return false;
    return m_currentButtons[glfwButton];
}

bool MouseInput::IsButtonPressed(int glfwButton) const {
    if (glfwButton < 0 || glfwButton >= kButtonCount) return false;
    return m_currentButtons[glfwButton] && !m_previousButtons[glfwButton];
}

void MouseInput::SetCaptured(bool captured) {
    if (m_captured == captured) return;

    m_captured = captured;
    glfwSetInputMode(
        m_window,
        GLFW_CURSOR,
        captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL
    );

    if (captured) {
        // Mark that the next Update() should seed lastX/Y rather than compute a delta.
        m_skipNext = true;
        spdlog::info("[MouseInput] Cursor captured — mouse look active");
    } else {
        m_deltaX = 0.0f;
        m_deltaY = 0.0f;
        spdlog::info("[MouseInput] Cursor released");
    }
}
