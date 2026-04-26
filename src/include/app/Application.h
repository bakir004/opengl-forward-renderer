#pragma once
#include <cstddef>
#include <memory>
#include <vector>

struct GLFWwindow;
class Renderer;
class IInputProvider;
class InputManager;
class Scene;
class RendererUI;

/// Top-level application class. Owns the GLFW window and the Renderer.
/// Manages the full lifetime of the window, GL context, and main loop.
class Application {
public:
    Application();

    /// Destroys the window, terminates GLFW, and shuts down the renderer.
    ~Application();

    /// Creates the GLFW window, sets up the GL context, and initializes the Renderer.
    /// Reads window settings (size, title, vsync) from config/settings.json via Options.
    /// @return true on success, false if GLFW or GLAD initialization fails
    bool Initialize();

    /// Runs the main loop, driving the given scene each frame.
    /// Calls scene.OnUpdate() then renders all objects the scene contains.
    void Run(Scene &scene);

    /// Runs the main loop with runtime scene switching.
    /// Press numeric keys 1..9 to switch to scenes at matching indices.
    /// @param m_scenes Ordered list of scene pointers. Null entries are ignored.
    /// @param initialSceneIndex Index of the scene to start with.
    void Run(const std::vector<Scene *> &m_scenes, std::size_t initialSceneIndex = 0);

    void ToggleFullscreen();

    /// Executes one frame: poll events, update input, tick scene, render, swap.
    /// Run() calls this in a loop; expose it here for custom loop control.
    void Update(Scene &scene);

    /// Returns a non-owning pointer to the active Renderer.
    /// Used by the framebuffer resize callback to forward resize events.
    Renderer *GetRenderer() const { return m_renderer.get(); }

    /// Returns a non-owning pointer to the InputManager.
    InputManager *GetInputManager() const { return m_input.get(); }

    /// Returns the current framebuffer dimensions in pixels.
    void GetFramebufferSize(int &width, int &height) const;

private:
    GLFWwindow *m_window = nullptr;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<InputManager> m_input;
    std::unique_ptr<RendererUI> m_ui;
    float m_lastFrameTime = 0.0f;
    bool m_imguiInitialized = false;

    bool m_fullscreen = false;
    int m_windowedX, m_windowedY;
    int m_windowedW, m_windowedH;

    std::vector<Scene*> m_scenes;
    std::size_t m_activeSceneIndex = 0;

    /// Runs one complete frame for the given scene and scene list (shared by
    /// both Run() overloads to avoid code duplication).
    void RunFrame(Scene &scene,
                  const std::vector<Scene *> &scenes,
                  std::size_t &activeSceneIndex);

    void RunHotKeys();
};
