#include "Application.h"
#include "../utils/Options.h"
#include "../core/Renderer.h"
#include <GLFW/glfw3.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

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
    if (!glfwInit()) { spdlog::error("glfwInit failed"); return false; }

    Options options("config/settings.json");

#ifndef NDEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    m_window = glfwCreateWindow(options.window.width, options.window.height, options.window.title.c_str(), nullptr, nullptr);
    if (!m_window) { spdlog::error("glfwCreateWindow failed"); glfwTerminate(); return false; }

    glfwMakeContextCurrent(m_window);
    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, framebuffer_size_callback);
    glfwSwapInterval(options.window.vsync ? 1 : 0);

    return m_renderer->Initialize();
}

void Application::Run() {
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();
        m_renderer->RenderFrame();
        glfwSwapBuffers(m_window);
    }
}
