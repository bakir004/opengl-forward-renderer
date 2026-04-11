#pragma once
#include <cstddef>
#include <memory>
#include <vector>

struct GLFWwindow;
class Renderer;
class KeyboardInput;
class MouseInput;
class Scene;

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
        void Run(Scene& scene);

        /// Runs the main loop with runtime scene switching.
        /// Press numeric keys 1..9 to switch to scenes at matching indices.
        /// @param scenes Ordered list of scene pointers. Null entries are ignored.
        /// @param initialSceneIndex Index of the scene to start with.
        void Run(const std::vector<Scene*>& scenes, std::size_t initialSceneIndex = 0);

        /// Executes one frame: poll events, update input, tick scene, render, swap.
        /// Run() calls this in a loop; expose it here for custom loop control.
        void Update(Scene& scene);

        /// Returns a non-owning pointer to the active Renderer.
        /// Used by the framebuffer resize callback to forward resize events.
        Renderer* GetRenderer() const { return m_renderer.get(); }

        /// Returns a non-owning pointer to the MouseInput.
        MouseInput* GetMouseInput() const { return m_mouse.get(); }

        /// Returns the current framebuffer dimensions in pixels.
        void GetFramebufferSize(int& width, int& height) const;

    private:
        GLFWwindow* m_window = nullptr;
        std::unique_ptr<Renderer>      m_renderer;
        std::unique_ptr<KeyboardInput> m_input;
        std::unique_ptr<MouseInput>    m_mouse;
        float                          m_lastFrameTime = 0.0f;
        bool                           m_imguiInitialized = false;
        bool                           m_wireframeOverride = false;
};
