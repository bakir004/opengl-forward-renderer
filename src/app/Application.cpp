#include "Application.h"
#include "../core/Renderer.h"
#include <GLFW/glfw3.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <fstream>

static void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    auto app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (app) app->GetRenderer()->Resize(width, height);
}

Application::Application() : m_renderer(std::make_unique<Renderer>()) {}
Application::~Application() = default;

bool Application::Initialize() {
    std::ifstream f("config/settings.json");
    if (!f.is_open()) { spdlog::error("Failed to open config/settings.json"); return false; }
    const auto cfg = nlohmann::json::parse(f);

    if (!glfwInit()) { spdlog::error("glfwInit failed"); return false; }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifndef NDEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

    const int w = cfg["window"]["width"];
    const int h = cfg["window"]["height"];
    m_window = glfwCreateWindow(w, h, "Forward Renderer", nullptr, nullptr);
    if (!m_window) { spdlog::error("glfwCreateWindow failed"); glfwTerminate(); return false; }

    glfwMakeContextCurrent(m_window);
    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, framebuffer_size_callback);
    glfwSwapInterval(cfg["window"]["vsync"] ? 1 : 0);

    return m_renderer->Initialize(cfg);
}

void Application::Run() {
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();
        m_renderer->RenderFrame();
        glfwSwapBuffers(m_window);
    }
}

void Application::Shutdown() {
    m_renderer->Shutdown();
    if (m_window) { glfwDestroyWindow(m_window); m_window = nullptr; }
    glfwTerminate();
}