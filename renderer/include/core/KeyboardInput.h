#pragma once
#include <array>

struct GLFWwindow;

/// Thin polling wrapper around GLFW keyboard state.
///
/// Responsibilities:
///   - Polls all valid GLFW key codes each frame via glfwGetKey.
///   - Tracks per-frame transitions (just-pressed / just-released).
///   - Logs each key the frame it is first pressed via spdlog.
///
/// Out of scope (handled by the caller):
///   - Camera or scene logic — callers query IsKeyDown / IsKeyPressed and act accordingly.
///   - Key remapping, action binding, or gamepad input.
///
/// Usage:
/// @code
///   KeyboardInput input(window);
///   // each frame, after glfwPollEvents():
///   input.Update();
///   if (input.IsKeyDown(GLFW_KEY_W)) camera.Move(CameraDirection::Forward, speed, dt);
/// @endcode
class KeyboardInput {
public:
    /// @param window  The GLFW window whose key state will be polled.
    explicit KeyboardInput(GLFWwindow* window);

    /// Polls GLFW for all valid key codes.
    /// Must be called once per frame, after glfwPollEvents().
    /// Logs any key that transitions from released to pressed this frame.
    void Update();

    /// Returns true if the given GLFW key is currently held down.
    /// @param glfwKey  A GLFW_KEY_* constant (e.g. GLFW_KEY_W).
    [[nodiscard]] bool IsKeyDown(int glfwKey) const;

    /// Returns true only on the single frame the key transitioned from up to down.
    /// Use this for one-shot actions (toggle, fire) rather than continuous movement.
    /// @param glfwKey  A GLFW_KEY_* constant.
    [[nodiscard]] bool IsKeyPressed(int glfwKey) const;

private:
    /// Total number of GLFW key slots (GLFW_KEY_LAST + 1 = 349).
    static constexpr int kKeyCount = 349;

    /// Returns a human-readable name for a GLFW key.
    /// Delegates to glfwGetKeyName for printable keys; uses a manual table for the rest.
    static const char* KeyName(int glfwKey);

    GLFWwindow* m_window = nullptr;

    std::array<bool, kKeyCount> m_current  = {};  ///< Key state this frame.
    std::array<bool, kKeyCount> m_previous = {};  ///< Key state last frame.
};
