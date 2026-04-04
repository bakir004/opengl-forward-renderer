#include "app/Application.h"
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

/// GLFW callback fired when the framebuffer is resized (e.g. window resize or DPI change).
/// Forwards the new dimensions to the Renderer so the GL viewport stays in sync.
static void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    auto app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (app) app->GetRenderer()->Resize(width, height);
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
    m_lastSubmittedItems = 0;
    for (const auto& item : submission.objects) {
        RenderItem drawItem = item;
        if (m_wireframeOverride)
            drawItem.drawMode = DrawMode::Wireframe;

        m_renderer->SubmitDraw(drawItem);
        ++m_lastSubmittedItems;
    }

    m_renderer->EndFrame();

    if (m_imguiInitialized) {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowBgAlpha(0.85f);
        if (ImGui::Begin("Renderer Debug")) {
            const float frameMs = dt * 1000.0f;
            const float fps = (dt > 0.0f) ? (1.0f / dt) : 0.0f;

            ImGui::Text("Frame time: %.2f ms", frameMs);
            ImGui::Text("FPS: %.1f", fps);
            ImGui::Separator();

            if (submission.camera) {
                const glm::vec3 cameraPos = submission.camera->GetPosition();
                ImGui::Text("Camera pos: %.2f, %.2f, %.2f", cameraPos.x, cameraPos.y, cameraPos.z);
                ImGui::Text("Camera yaw/pitch: %.1f / %.1f",
                            submission.camera->GetYaw(),
                            submission.camera->GetPitch());
            } else {
                ImGui::TextUnformatted("Camera: none");
            }

            ImGui::Separator();
            ImGui::Text("Framebuffer: %d x %d", w, h);
            ImGui::Checkbox("Wireframe override", &m_wireframeOverride);
            ImGui::Text("Submitted render items: %d", static_cast<int>(m_lastSubmittedItems));

            if (m_mouse && m_mouse->IsCaptured())
                ImGui::TextUnformatted("Hint: Press TAB to release mouse for UI interaction");
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
