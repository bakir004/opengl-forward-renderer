#pragma once
#include <functional>
#include <memory>

struct GLFWwindow;
class Renderer;
class KeyboardInput;
class MouseInput;

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

        /// Runs the main loop: polls events, renders a frame, and swaps buffers each iteration.
        /// Calls \p onRender each frame between BeginFrame and EndFrame.
        /// Exits when the window is closed.
        ///
        /// @param onRender Called each frame with the active Renderer and elapsed time in seconds.
        void Run(std::function<void(Renderer&, float)> onRender = nullptr);

        /// Returns a non-owning pointer to the active Renderer.
        /// Used by the framebuffer resize callback to forward resize events.
        Renderer* GetRenderer() const { return m_renderer.get(); }

        /// Returns a non-owning pointer to the KeyboardInput handler.
        /// Valid after Initialize() returns true.
        KeyboardInput* GetKeyboardInput() const { return m_input.get(); }

        /// Returns a non-owning pointer to the MouseInput handler.
        /// Valid after Initialize() returns true.
        MouseInput* GetMouseInput() const { return m_mouse.get(); }

    private:
        GLFWwindow* m_window = nullptr;
        std::unique_ptr<Renderer>      m_renderer;
        std::unique_ptr<KeyboardInput> m_input;
        std::unique_ptr<MouseInput>    m_mouse;
};
