#include "app/Application.h"
#include "assets/AssetImporter.h"
#include "core/MeshBuffer.h"
#include "scene/Scene.h"
#include "scene/FrameSubmission.h"
#include "scene/RenderItem.h"
#include "core/Camera.h"
#include "utils/Options.h"
#include "core/Renderer.h"
#include "core/KeyboardInput.h"
#include "core/MouseInput.h"
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string>

static void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    auto app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (app) app->GetRenderer()->Resize(width, height);
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    auto app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (app && app->GetMouseInput()) {
        app->GetMouseInput()->OnScroll(static_cast<float>(yoffset));
    }
}

static std::string FormatCompactCount(uint64_t value) {
    if (value >= 1000000ull) {
        return std::to_string(value / 1000000ull) + "." +
               std::to_string((value % 1000000ull) / 100000ull) + "M";
    }
    if (value >= 1000ull) {
        return std::to_string(value / 1000ull) + "." +
               std::to_string((value % 1000ull) / 100ull) + "k";
    }
    return std::to_string(value);
}

Application::Application() : m_renderer(std::make_unique<Renderer>()) {}
Application::~Application() {
    if (m_imguiInitialized) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        m_imguiInitialized = false;
    }

    m_renderer->Shutdown();
    if (m_window) { glfwDestroyWindow(m_window); m_window = nullptr; }
    glfwTerminate();
}

bool Application::Initialize() {
#ifndef NDEBUG
    spdlog::set_level(spdlog::level::debug);
#endif

    if (!glfwInit()) {
        spdlog::error("[Application] GLFW initialization failed — glfwInit() returned false");
        return false;
    }

    Options options("config/settings.json");

#ifndef NDEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

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
    glfwSetScrollCallback(m_window, scroll_callback);

    int vsyncInterval = options.window.vsync ? 1 : 0;
    glfwSwapInterval(vsyncInterval);
    spdlog::info("[Application] VSync {}", vsyncInterval ? "enabled" : "disabled");

    if (!m_renderer->Initialize())
        return false;

    m_input = std::make_unique<KeyboardInput>(m_window);
    spdlog::info("[Application] KeyboardInput initialized");

    m_mouse = std::make_unique<MouseInput>(m_window);
    spdlog::info("[Application] MouseInput initialized");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    if (!ImGui_ImplGlfw_InitForOpenGL(m_window, true)) {
        spdlog::warn("[Application] ImGui GLFW backend init failed — debug overlay disabled");
        ImGui::DestroyContext();
    } else if (!ImGui_ImplOpenGL3_Init("#version 330")) {
        spdlog::warn("[Application] ImGui OpenGL backend init failed — debug overlay disabled");
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    } else {
        m_imguiInitialized = true;
        spdlog::info("[Application] ImGui debug overlay initialized");
    }

    return true;
}

void Application::GetFramebufferSize(int& width, int& height) const {
    glfwGetFramebufferSize(m_window, &width, &height);
}

void Application::Update(Scene& scene) {
    glfwPollEvents();
    m_input->Update();
    m_mouse->Update();

    int w = 0, h = 0;
    GetFramebufferSize(w, h);

    const float currTime = static_cast<float>(glfwGetTime());
    const float dt = (m_lastFrameTime > 0.0f) ? (currTime - m_lastFrameTime) : 0.0f;
    m_lastFrameTime = currTime;

    scene.InternalUpdate(dt, *m_input, *m_mouse, w, h);

    FrameSubmission submission;
    scene.BuildSubmission(submission);
    submission.clearInfo.viewport = {0, 0, w, h};
    submission.time      = currTime;
    submission.deltaTime = dt;

    m_renderer->BeginFrame(submission);
    for (const auto& item : submission.objects) {
        RenderItem drawItem = item;
        if (m_wireframeOverride)
            drawItem.drawMode = DrawMode::Wireframe;

        m_renderer->SubmitDraw(drawItem);
    }

    m_renderer->EndFrame();

    if (m_imguiInitialized) {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.87f);

        if (ImGui::Begin("Renderer Debug")) {
            const RendererDebugStats& stats = m_renderer->GetDebugStats();
            const AssetCacheStats cacheStats = AssetImporter::GetCacheStats();
            // ── Performance ───────────────────────────────────────────────
            ImGui::SeparatorText("Performance");
            ImGui::Text("Frame time : %.2f ms", stats.frameTimeMs);
            ImGui::Text("FPS        : %.1f", stats.fps);

            // ── Camera ────────────────────────────────────────────────────
            ImGui::SeparatorText("Camera");
            if (submission.camera) {
                const glm::vec3 pos = submission.camera->GetPosition();
                ImGui::Text("Position   : %.2f, %.2f, %.2f", pos.x, pos.y, pos.z);
                ImGui::Text("Yaw / Pitch: %.1f / %.1f",
                            submission.camera->GetYaw(),
                            submission.camera->GetPitch());

                const char* modeStr = "FreeFly";
                switch (submission.camera->GetMode()) {
                    case CameraMode::FirstPerson:  modeStr = "FirstPerson";  break;
                    case CameraMode::ThirdPerson:  modeStr = "ThirdPerson";  break;
                    default: break;
                }
                ImGui::Text("Mode       : %s", modeStr);
                ImGui::Text("Speed      : %.1f m/s", scene.GetCurrentCameraSpeed());
            } else {
                ImGui::TextDisabled("No camera");
            }

            // ── Scene ─────────────────────────────────────────────────────
            ImGui::SeparatorText("Renderer Stats");
            ImGui::Text("Framebuffer: %d x %d", w, h);
            ImGui::Text("Submitted  : %u", stats.submittedRenderItemCount);
            ImGui::Text("Queued     : %u", stats.queuedRenderItemCount);
            ImGui::Text("Processed  : %u", stats.processedRenderItemCount);
            ImGui::Text("Draw calls : %u", stats.drawCallCount);
            ImGui::Text("Approx tris: %s", FormatCompactCount(stats.approxTriangleCount).c_str());
            if (stats.approxTriangleCountApproximate)
                ImGui::TextDisabled("Triangle estimate uses index/vertex counts and treats non-triangle topologies as 0.");

            ImGui::SeparatorText("Lighting Debug");
            ImGui::Text("Directional : %u", stats.directionalLightCount);
            ImGui::Text("Point lights: %u", stats.pointLightCount);
            ImGui::Text("Shadow cast : %u", stats.shadowCasterCount);
            if (stats.shadowCasterCountApproximate)
                ImGui::TextDisabled("Shadow caster count is derived from queued RenderItem flags.");
            if (!stats.shadowPassDataAvailable)
                ImGui::TextDisabled("Shadow pass debug data is not wired in this phase.");

            ImGui::SeparatorText("Resource Cache");
            ImGui::Text("Cached total: %zu", cacheStats.TotalCount());
            ImGui::Text("Shaders     : %zu", cacheStats.shaderCount);
            ImGui::Text("Textures    : %zu", cacheStats.textureCount);
            ImGui::Text("Meshes      : %zu", cacheStats.meshCount);
            ImGui::Text("Materials   : %zu", cacheStats.materialCount);

            // ── Renderer ──────────────────────────────────────────────────
            ImGui::SeparatorText("Renderer");
            ImGui::Checkbox("Wireframe override", &m_wireframeOverride);

            // ── Controls hint ─────────────────────────────────────────────
            if (m_mouse && m_mouse->IsCaptured()) {
                ImGui::Spacing();
                ImGui::TextDisabled("TAB — release mouse for UI");
            }
        }
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    glfwSwapBuffers(m_window);
}

void Application::Run(Scene& scene) {
    while (!glfwWindowShouldClose(m_window))
        Update(scene);
}

void Application::Run(const std::vector<Scene*>& scenes, std::size_t initialSceneIndex) {
    if (scenes.empty()) {
        spdlog::warn("[Application] Run(scenes): no scenes provided");
        return;
    }

    std::size_t activeSceneIndex = initialSceneIndex;
    if (activeSceneIndex >= scenes.size() || scenes[activeSceneIndex] == nullptr)
        activeSceneIndex = 0;

    while (activeSceneIndex < scenes.size() && scenes[activeSceneIndex] == nullptr)
        ++activeSceneIndex;

    if (activeSceneIndex >= scenes.size()) {
        spdlog::warn("[Application] Run(scenes): all scene entries are null");
        return;
    }

    spdlog::info("[Application] Active scene index: {}", activeSceneIndex);
    while (!glfwWindowShouldClose(m_window)) {
        Update(*scenes[activeSceneIndex]);

        const std::size_t maxSwitchableScenes = std::min<std::size_t>(9, scenes.size());
        for (std::size_t i = 0; i < maxSwitchableScenes; ++i) {
            if (scenes[i] == nullptr) continue;
            const int key = GLFW_KEY_1 + static_cast<int>(i);
            if (m_input->IsKeyPressed(key) && activeSceneIndex != i) {
                activeSceneIndex = i;
                spdlog::info("[Application] Switched to scene index {} (key {})", i, i + 1);
                break;
            }
        }
    }
}
