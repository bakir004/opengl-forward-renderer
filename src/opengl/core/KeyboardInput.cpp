#include "core/KeyboardInput.h"
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

KeyboardInput::KeyboardInput(GLFWwindow* window) : m_window(window) {}

void KeyboardInput::Update() {
    m_previous = m_current;
    m_current  = {};

    // Poll printable keys: SPACE (32) through GRAVE_ACCENT (96).
    for (int k = GLFW_KEY_SPACE; k <= GLFW_KEY_GRAVE_ACCENT; ++k) {
        m_current[k] = (glfwGetKey(m_window, k) == GLFW_PRESS);
        if (m_current[k] && !m_previous[k])
            spdlog::debug("[KeyboardInput] Key pressed: {}", KeyName(k));
    }

    // Poll special/function keys: ESCAPE (256) through MENU (348).
    for (int k = GLFW_KEY_ESCAPE; k <= GLFW_KEY_LAST; ++k) {
        m_current[k] = (glfwGetKey(m_window, k) == GLFW_PRESS);
        if (m_current[k] && !m_previous[k])
            spdlog::debug("[KeyboardInput] Key pressed: {}", KeyName(k));
    }
}

bool KeyboardInput::IsKeyDown(int glfwKey) const {
    if (glfwKey < 0 || glfwKey >= kKeyCount) return false;
    return m_current[glfwKey];
}

bool KeyboardInput::IsKeyPressed(int glfwKey) const {
    if (glfwKey < 0 || glfwKey >= kKeyCount) return false;
    return m_current[glfwKey] && !m_previous[glfwKey];
}

const char* KeyboardInput::KeyName(int glfwKey) {
    // glfwGetKeyName covers all printable keys (letters, digits, punctuation).
    const char* name = glfwGetKeyName(glfwKey, 0);
    if (name) return name;

    switch (glfwKey) {
        case GLFW_KEY_SPACE:           return "SPACE";
        case GLFW_KEY_ESCAPE:          return "ESCAPE";
        case GLFW_KEY_ENTER:           return "ENTER";
        case GLFW_KEY_TAB:             return "TAB";
        case GLFW_KEY_BACKSPACE:       return "BACKSPACE";
        case GLFW_KEY_INSERT:          return "INSERT";
        case GLFW_KEY_DELETE:          return "DELETE";
        case GLFW_KEY_HOME:            return "HOME";
        case GLFW_KEY_END:             return "END";
        case GLFW_KEY_PAGE_UP:         return "PAGE_UP";
        case GLFW_KEY_PAGE_DOWN:       return "PAGE_DOWN";
        case GLFW_KEY_UP:              return "UP";
        case GLFW_KEY_DOWN:            return "DOWN";
        case GLFW_KEY_LEFT:            return "LEFT";
        case GLFW_KEY_RIGHT:           return "RIGHT";
        case GLFW_KEY_LEFT_SHIFT:      return "LEFT_SHIFT";
        case GLFW_KEY_RIGHT_SHIFT:     return "RIGHT_SHIFT";
        case GLFW_KEY_LEFT_CONTROL:    return "LEFT_CTRL";
        case GLFW_KEY_RIGHT_CONTROL:   return "RIGHT_CTRL";
        case GLFW_KEY_LEFT_ALT:        return "LEFT_ALT";
        case GLFW_KEY_RIGHT_ALT:       return "RIGHT_ALT";
        case GLFW_KEY_LEFT_SUPER:      return "LEFT_SUPER";
        case GLFW_KEY_RIGHT_SUPER:     return "RIGHT_SUPER";
        case GLFW_KEY_CAPS_LOCK:       return "CAPS_LOCK";
        case GLFW_KEY_SCROLL_LOCK:     return "SCROLL_LOCK";
        case GLFW_KEY_NUM_LOCK:        return "NUM_LOCK";
        case GLFW_KEY_PRINT_SCREEN:    return "PRINT_SCREEN";
        case GLFW_KEY_PAUSE:           return "PAUSE";
        case GLFW_KEY_F1:              return "F1";
        case GLFW_KEY_F2:              return "F2";
        case GLFW_KEY_F3:              return "F3";
        case GLFW_KEY_F4:              return "F4";
        case GLFW_KEY_F5:              return "F5";
        case GLFW_KEY_F6:              return "F6";
        case GLFW_KEY_F7:              return "F7";
        case GLFW_KEY_F8:              return "F8";
        case GLFW_KEY_F9:              return "F9";
        case GLFW_KEY_F10:             return "F10";
        case GLFW_KEY_F11:             return "F11";
        case GLFW_KEY_F12:             return "F12";
        default:                       return "UNKNOWN";
    }
}
