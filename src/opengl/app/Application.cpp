#include "app/Application.h"
#include "ui/RendererUI.h"
#include "assets/AssetImporter.h"
#include "core/MeshBuffer.h"
#include "scene/Scene.h"
#include "scene/FrameSubmission.h"
#include "scene/RenderItem.h"
#include "core/Camera.h"
#include "utils/Options.h"
#include "core/Renderer.h"
#include "core/InputManager.h"
#include "core/MouseInput.h"
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <scene/LightEnvironment.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// GLFW callbacks
// ─────────────────────────────────────────────────────────────────────────────

// NOTE: We no longer resize the renderer directly from the framebuffer callback
// because the renderer viewport is a sub-region of the window (sidebar excluded).
// RendererUI::Draw() calls renderer.Resize(vpW, vpH) each frame instead.
// We keep the callback only to handle GL context invalidation on some platforms.
static void framebuffer_size_callback(GLFWwindow * /*window*/, int /*w*/, int /*h*/) {
    // Intentionally empty — resize is driven by RendererUI each frame.
}

static void scroll_callback(GLFWwindow *window, double /*xoff*/, double yoff) {
    auto *app = static_cast<Application *>(glfwGetWindowUserPointer(window));
    if (!app || !app->GetInputManager()) return;
    app->GetInputManager()->GetMouse().OnScroll(static_cast<float>(yoff));
}

// ─────────────────────────────────────────────────────────────────────────────
// Ctor / Dtor
// ─────────────────────────────────────────────────────────────────────────────
Application::Application()
    : m_renderer(std::make_unique<Renderer>())
      , m_ui(std::make_unique<RendererUI>()) {
}

Application::~Application() {
    if (m_imguiInitialized) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        m_imguiInitialized = false;
    }
    m_renderer->Shutdown();
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
}

// ─────────────────────────────────────────────────────────────────────────────
// Initialize
// ─────────────────────────────────────────────────────────────────────────────
bool Application::Initialize() {
#ifndef NDEBUG
    spdlog::set_level(spdlog::level::debug);
#endif
    if (!glfwInit()) {
        spdlog::error("[Application] glfwInit() failed");
        return false;
    }

    Options options("config/settings.json");
#ifndef NDEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

    struct GLVer {
        int major, minor;
    };
    const GLVer versions[] = {{4, 6}, {4, 5}, {4, 4}, {4, 3}, {4, 2}, {4, 1}, {4, 0}, {3, 3}};
    int cMaj = 0, cMin = 0;
    for (const auto &v: versions) {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, v.major);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, v.minor);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
        glfwWindowHint(GLFW_DEPTH_BITS, 24);
        m_window = glfwCreateWindow(options.window.width, options.window.height,
                                    options.window.title.c_str(), nullptr, nullptr);
        if (m_window) {
            cMaj = v.major;
            cMin = v.minor;
            break;
        }
    }
    if (!m_window) {
        spdlog::error("[Application] Failed to create GLFW window (tried GL 4.6–3.3)");
        glfwTerminate();
        return false;
    }
    spdlog::info("[Application] Window {}x{}, OpenGL {}.{} Core",
                 options.window.width, options.window.height, cMaj, cMin);

    glfwMakeContextCurrent(m_window);
    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, framebuffer_size_callback);
    glfwSetScrollCallback(m_window, scroll_callback);
    glfwSwapInterval(options.window.vsync ? 1 : 0);
    spdlog::info("[Application] VSync {}", options.window.vsync ? "on" : "off");

    if (!m_renderer->Initialize()) return false;

    m_input = std::make_unique<InputManager>(m_window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    if (!ImGui_ImplGlfw_InitForOpenGL(m_window, true) ||
        !ImGui_ImplOpenGL3_Init("#version 330")) {
        spdlog::warn("[Application] ImGui init failed — UI disabled");
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    } else {
        m_imguiInitialized = true;
        spdlog::info("[Application] ImGui initialized");
    }
    return true;
}

void Application::GetFramebufferSize(int &w, int &h) const {
    glfwGetFramebufferSize(m_window, &w, &h);
}

// ─────────────────────────────────────────────────────────────────────────────
// RunFrame  — single shared frame body used by both Run() overloads
// ─────────────────────────────────────────────────────────────────────────────
void Application::RunFrame(Scene &scene,
                           const std::vector<Scene *> &scenes,
                           std::size_t &activeSceneIndex) {
    glfwPollEvents();
    m_input->Update();

    int fbW = 0, fbH = 0;
    GetFramebufferSize(fbW, fbH);

    const float now = static_cast<float>(glfwGetTime());
    const float dt = (m_lastFrameTime > 0.f) ? (now - m_lastFrameTime) : 0.f;
    m_lastFrameTime = now;

    scene.InternalUpdate(dt, *m_input, fbW, fbH);

    // Build submission
    FrameSubmission sub;
    scene.BuildSubmission(sub);
    sub.time = now;
    sub.deltaTime = dt;

    // Use the viewport rect computed by RendererUI (sidebar already excluded).
    // GL's glViewport origin is bottom-left, so:
    //   x = sidebar + handle width  (pixels from left)
    //   y = 0                       (bottom of framebuffer; topbar is ImGui-only)
    //   w, h = remaining area
    // On the very first frame m_ui viewport equals (0,0,fbW,fbH) which is fine.
    int vpX, vpY, vpW, vpH;
    m_ui->GetViewportRect(vpX, vpY, vpW, vpH);
    sub.clearInfo.viewport = {
        vpX, 0, // y=0: GL bottom-left origin
        vpW > 0 ? vpW : fbW,
        vpH > 0 ? vpH : fbH
    };

    m_renderer->BeginFrame(sub);
    for (const auto &item: sub.objects) {
        RenderItem di = item;
        if (m_ui->wireframeOverride) di.drawMode = DrawMode::Wireframe;
        m_renderer->SubmitDraw(di);
    }
    m_renderer->EndFrame();

    // ── ImGui ─────────────────────────────────────────────────────────────────
    if (m_imguiInitialized) {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // RendererUI::Draw() also calls renderer.Resize(vpW, vpH) internally
        // so the GL viewport stays correct even as the sidebar is resized.
        m_ui->Draw(fbW, fbH, scene, *m_renderer, &m_input->GetMouse(),
                   scenes, activeSceneIndex);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    glfwSwapBuffers(m_window);
}

// ─────────────────────────────────────────────────────────────────────────────
// Update  (single-scene variant called by external custom loops)
// ─────────────────────────────────────────────────────────────────────────────
void Application::Update(Scene &scene) {
    std::vector<Scene *> sv = {&scene};
    std::size_t idx = 0;
    RunFrame(scene, sv, idx);
}

// ─────────────────────────────────────────────────────────────────────────────
// Run (single scene)
// ─────────────────────────────────────────────────────────────────────────────
void Application::Run(Scene &scene) {
    std::vector<Scene *> sv = {&scene};
    std::size_t idx = 0;
    while (!glfwWindowShouldClose(m_window)) {
        RunFrame(scene, sv, idx);
        runHotKeys();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Run (multiple scenes, keyboard 1–9 + topbar buttons switch scenes)
// ─────────────────────────────────────────────────────────────────────────────
void Application::Run(const std::vector<Scene *> &scenes, std::size_t initialIdx) {
    if (scenes.empty()) {
        spdlog::warn("[Application] No scenes");
        return;
    }

    std::size_t active = initialIdx;
    if (active >= scenes.size() || !scenes[active]) active = 0;
    while (active < scenes.size() && !scenes[active]) ++active;
    if (active >= scenes.size()) {
        spdlog::warn("[Application] All scenes null");
        return;
    }

    spdlog::info("[Application] Starting at scene {}", active);

    while (!glfwWindowShouldClose(m_window)) {
        RunFrame(*scenes[active], scenes, active);

        // Keyboard shortcuts 1–9 (supplement the topbar buttons).
        const std::size_t maxK = std::min<std::size_t>(9, scenes.size());
        for (std::size_t i = 0; i < maxK; ++i) {
            if (!scenes[i]) continue;
            if (m_input->IsKeyPressed(GLFW_KEY_1 + static_cast<int>(i)) && active != i) {
                active = i;
                spdlog::info("[Application] Switched to scene {} (key {})", i, i + 1);
                break;
            }
        }
        runHotKeys();
    }
}

void Application::runHotKeys() {
    if (m_input->IsKeyPressed(GLFW_KEY_F11)) {
        ToggleFullscreen();
        spdlog::info("[Application] Toggle fullscreen: {}", m_fullscreen);
    }

    if (m_input->IsKeyPressed(GLFW_KEY_X)) {
        m_ui->showSidebar = !m_ui->showSidebar;
        spdlog::debug("[Application] Toggle sidebar (inspector): {}", m_ui->showSidebar);
    }

    if (m_input->IsKeyPressed(GLFW_KEY_Y)) {
        m_ui->wireframeOverride = !m_ui->wireframeOverride;
        spdlog::debug("[Application] Toggle wireframe mode: {}", m_ui->wireframeOverride);
    }

    if (m_input->IsKeyPressed(GLFW_KEY_H)) {
        m_ui->showHelpWindow = !m_ui->showHelpWindow;
        spdlog::debug("[Application] Toggle help window: {}", m_ui->showHelpWindow);
    }
}

void Application::ToggleFullscreen() {
    m_fullscreen = !m_fullscreen;

    if (m_fullscreen) {
        // Save windowed position + size
        glfwGetWindowPos(m_window, &m_windowedX, &m_windowedY);
        glfwGetWindowSize(m_window, &m_windowedW, &m_windowedH);

        GLFWmonitor *monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode *mode = glfwGetVideoMode(monitor);

        glfwSetWindowMonitor(
            m_window,
            monitor,
            0, 0,
            mode->width,
            mode->height,
            mode->refreshRate
        );
    } else {
        glfwSetWindowMonitor(
            m_window,
            nullptr,
            m_windowedX,
            m_windowedY,
            m_windowedW,
            m_windowedH,
            0
        );
    }
}
