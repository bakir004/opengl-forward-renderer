#include "Application.h"
#include "../utils/Options.h"
#include "../core/Renderer.h"
#include "../core/KeyboardInput.h"
#include "../core/MouseInput.h"
#include "../scene/FrameSubmission.h"
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
    m_renderer->Shutdown();
    if (m_window) { glfwDestroyWindow(m_window); m_window = nullptr; }
    glfwTerminate();
}

bool Application::Initialize() {
    if (!glfwInit()) {
        spdlog::error("[Application] GLFW initialization failed — glfwInit() returned false");
        return false;
    }

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
    int createdMajor = 0, createdMinor = 0;
    for (const auto& v : versions) {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, v.major);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, v.minor);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

        glfwWindowHint(GLFW_DEPTH_BITS, 24);
        m_window = glfwCreateWindow(options.window.width, options.window.height, options.window.title.c_str(), nullptr, nullptr);
        if (m_window) {
            windowCreated = true;
            createdMajor = v.major;
            createdMinor = v.minor;
            break;
        }
    }

    if (!windowCreated) {
        spdlog::error("[Application] Failed to create GLFW window — no supported OpenGL context available (tried 4.6 down to 3.3 Core Profile)");
        glfwTerminate();
        return false;
    }

    spdlog::info("[Application] Window created ({}x{}, OpenGL {}.{} Core Profile)",
        options.window.width, options.window.height, createdMajor, createdMinor);

    glfwMakeContextCurrent(m_window);
    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, framebuffer_size_callback);

    int vsyncInterval = options.window.vsync ? 1 : 0;
    glfwSwapInterval(vsyncInterval);
    spdlog::info("[Application] VSync {}", vsyncInterval ? "enabled" : "disabled");

    if (!m_renderer->Initialize())
        return false;

    m_input = std::make_unique<KeyboardInput>(m_window);
    spdlog::info("[Application] KeyboardInput initialized");

    m_mouse = std::make_unique<MouseInput>(m_window);
    spdlog::info("[Application] MouseInput initialized");

    return true;
}
void Application::GetFramebufferSize(int& width, int& height) const {
    glfwGetFramebufferSize(m_window, &width, &height);
}
void Application::Run(std::function<void(Renderer&, float)> onRender) {
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();
        m_input->Update();
        m_mouse->Update();

        FrameParams frame{};
        frame.clearColor = { 0.08f, 0.09f, 0.12f, 1.0f };
        m_renderer->BeginFrame(frame);

        if (onRender)
            onRender(*m_renderer, static_cast<float>(glfwGetTime()));

        m_renderer->EndFrame();
        glfwSwapBuffers(m_window);
    }
}

void Application::Run(std::function<void(FrameSubmission&, float)> onRender) {
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();
        m_input->Update();
        m_mouse->Update();

        int width = 0, height = 0;
        GetFramebufferSize(width, height);

        FrameSubmission submission;
        submission.camera = nullptr; // caller will set camera
        submission.viewport = {0, 0, width, height};
        float currTime = static_cast<float>(glfwGetTime());
        submission.time = currTime;
        submission.deltaTime = (m_lastFrameTime > 0.0f) ? (currTime - m_lastFrameTime) : 0.0f;
        m_lastFrameTime = currTime;

        if (onRender)
            onRender(submission, submission.time);

        if (submission.camera)
            m_renderer->BeginFrame(submission);
        else {
            FrameParams frame{};
            frame.clearColor = submission.clearColor;
            frame.clearFlags = submission.clearFlags;
            m_renderer->BeginFrame(frame);
        }

        for (const auto& item : submission.objects) {
            m_renderer->SubmitDraw(item);
        }

        m_renderer->EndFrame();
        glfwSwapBuffers(m_window);
    }
}
