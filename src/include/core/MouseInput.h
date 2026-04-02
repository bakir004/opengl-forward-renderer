#pragma once

struct GLFWwindow;

/// Polling wrapper for mouse cursor movement.
///
/// Responsibilities:
///   - Computes per-frame cursor delta via glfwGetCursorPos.
///   - Manages cursor capture (hidden + confined) for FPS-style look.
///   - Suppresses the first delta after capture to prevent the initial position jump.
///
/// Out of scope (handled by the caller):
///   - Camera rotation — callers read GetDeltaX/Y and call Camera::Rotate().
///   - Sensitivity scaling — apply a scalar at the call site.
///
/// Usage:
/// @code
///   MouseInput mouse(window);
///   mouse.SetCaptured(true);
///   // each frame, after glfwPollEvents():
///   mouse.Update();
///   camera.Rotate(mouse.GetDeltaX() * sensitivity, -mouse.GetDeltaY() * sensitivity);
/// @endcode
class MouseInput {
public:
    /// @param window  The GLFW window to poll cursor state from.
    explicit MouseInput(GLFWwindow* window);

    /// Polls cursor position and computes delta from the previous frame.
    /// Must be called once per frame, after glfwPollEvents().
    /// Delta is zeroed when the cursor is not captured.
    void Update();

    /// Captures or releases the cursor.
    ///   - Captured: cursor hidden and locked to window center (GLFW_CURSOR_DISABLED).
    ///   - Released: cursor visible and free; deltas become zero.
    /// The delta for the first frame after capture is suppressed to avoid a jump.
    void SetCaptured(bool captured);

    /// Returns true if the cursor is currently captured.
    [[nodiscard]] bool IsCaptured() const { return m_captured; }

    /// Horizontal cursor delta this frame, in pixels. Positive = moved right.
    [[nodiscard]] float GetDeltaX() const { return m_deltaX; }

    /// Vertical cursor delta this frame, in pixels. Positive = moved down (screen space).
    /// Callers typically negate this when driving camera pitch (screen-down = pitch-down).
    [[nodiscard]] float GetDeltaY() const { return m_deltaY; }

private:
    GLFWwindow* m_window   = nullptr;
    double      m_lastX    = 0.0;
    double      m_lastY    = 0.0;
    float       m_deltaX   = 0.0f;
    float       m_deltaY   = 0.0f;
    bool        m_captured = false;
    bool        m_skipNext = false;  ///< Suppresses the first delta after capture.
};
