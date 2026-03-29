#include "Application.h"
#include "SampleScene.h"
#include "../utils/Options.h"
#include "../core/Renderer.h"
#include <GLFW/glfw3.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

/// GLFW callback fired when the framebuffer is resized (e.g. window resize or DPI change).
/// Forwards the new dimensions to the Renderer so the GL viewport stays in sync.
static void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    auto app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (app) app->GetRenderer()->Resize(width, height);
}

Application::Application() : m_renderer(std::make_unique<Renderer>()) {}
Application::~Application() {
    m_sampleScene.reset();
    m_renderer->Shutdown();
    if (m_window) { glfwDestroyWindow(m_window); m_window = nullptr; }
    glfwTerminate();
}

bool Application::Initialize() {
    if (!glfwInit()) { spdlog::error("glfwInit failed"); return false; }

    Options options("config/settings.json");

#ifndef NDEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

    // Try creating the highest GL context we support, falling back to older versions if needed.
    struct GLVersion { int major; int minor; };
    std::vector<GLVersion> versions = {
        {4, 6}, {4, 5}, {4, 4}, {4, 3}, {4, 2}, {4, 1}, {4, 0}, {3, 3}
    };

    bool windowCreated = false;
    for (const auto& v : versions) {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, v.major);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, v.minor);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
        
        m_window = glfwCreateWindow(options.window.width, options.window.height, "Forward Renderer", nullptr, nullptr);
        if (m_window) {
            windowCreated = true;
            break;
        }
    }

    if (!windowCreated) {
        spdlog::error("Failed to create any OpenGL context (3.3+ required)");
        glfwTerminate();
        return false;
    }

    // Make the GL context current and set up our callback before initializing the renderer, so it can make GL calls and log as needed.
    glfwMakeContextCurrent(m_window);
    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, framebuffer_size_callback);
    glfwSwapInterval(options.window.vsync ? 1 : 0);

    if (!m_renderer->Initialize())
        return false;

    m_sampleScene = std::make_unique<SampleScene>();
    if (!m_sampleScene->Setup("shaders/basic.vert", "shaders/basic.frag")) {
        spdlog::warn("SampleScene setup failed; scene will not draw");
        m_sampleScene.reset();
    }
    return true;
}

void Application::Run() {
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();

        // Sprint 3 will build a FrameParams from the active camera and
        // scene settings before passing it here.
        FrameParams frame{};
        frame.clearColor = { 0.08f, 0.09f, 0.12f, 1.0f };
        m_renderer->BeginFrame(frame);

        // Draw calls go here once ShaderProgram + MeshBuffer are wired up.
        if (m_sampleScene)
            m_sampleScene->Render(*m_renderer, static_cast<float>(glfwGetTime()));

        m_renderer->EndFrame();
        glfwSwapBuffers(m_window);
    }
}
