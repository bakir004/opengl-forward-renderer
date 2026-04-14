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
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string_view>
#include <string>

static void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    auto app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (app) app->GetRenderer()->Resize(width, height);
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    auto app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (!app || !app->GetMouseInput())
        return;

    // Route wheel input to camera only when UI is not actively consuming mouse input.
    // Exception: while cursor is captured for mouselook/game control, keep camera zoom active.
    const bool uiWantsMouse = ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse;
    const bool allowCameraScroll = app->GetMouseInput()->IsCaptured() || !uiWantsMouse;
    if (allowCameraScroll)
        app->GetMouseInput()->OnScroll(static_cast<float>(yoffset));
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

static void DrawDirectionalLightDebug(DirectionalLight& light) {
    ImGui::Text("Name      : %s", light.name.empty() ? "(unnamed)" : light.name.c_str());
    ImGui::Text("Enabled   : %s", light.enabled ? "yes" : "no");

    ImGui::DragFloat("Intensity", &light.intensity, 0.05f, 0.0f, 1000.0f, "%.3f");
    ImGui::ColorEdit3("Color", &light.color.x);

    // Edit direction as azimuth + elevation rather than raw XYZ.
    //
    // WHY: the direction must stay a unit vector. Editing raw XYZ and then
    // calling glm::normalize() couples all three components — dragging X
    // silently rescales Y and Z to restore length 1, making it impossible
    // to control one axis without the others jumping.
    //
    // Azimuth and elevation are orthogonal degrees of freedom: dragging one
    // never affects the other. The XYZ direction is derived from them, so it
    // is always unit length by construction (no normalize needed).
    const glm::vec3& d = light.direction;
    float elevation = glm::degrees(std::asin(glm::clamp(d.y, -1.0f, 1.0f)));
    float azimuth   = glm::degrees(std::atan2(d.z, d.x));

    bool dirChanged = false;
    dirChanged |= ImGui::DragFloat("Azimuth (°)",   &azimuth,   0.5f, -180.0f, 180.0f, "%.1f");
    dirChanged |= ImGui::DragFloat("Elevation (°)", &elevation, 0.5f,  -90.0f,  90.0f, "%.1f");

    if (dirChanged) {
        const float pitchRad = glm::radians(elevation);
        const float yawRad   = glm::radians(azimuth);
        light.direction = {
            std::cos(pitchRad) * std::cos(yawRad),
            std::sin(pitchRad),
            std::cos(pitchRad) * std::sin(yawRad)
        };
    }
    ImGui::TextDisabled("XYZ: %.3f, %.3f, %.3f", d.x, d.y, d.z);
}

// Returns true if the light should be removed.
static bool DrawPointLightDebug(PointLight& light, int index, const glm::vec3& cameraPos) {
    const std::string label = "Point Light " + std::to_string(index + 1);
    if (!ImGui::TreeNode(label.c_str()))
        return false;

    ImGui::Text("Name      : %s", light.name.empty() ? "(unnamed)" : light.name.c_str());
    ImGui::Text("Enabled   : %s", light.enabled ? "yes" : "no");

    ImGui::DragFloat("Intensity", &light.intensity, 0.05f, 0.0f, 1000.0f, "%.3f");
    ImGui::ColorEdit3("Color", &light.color.x);
    ImGui::DragFloat3("Position", &light.position.x, 0.05f);
    ImGui::DragFloat("Radius", &light.radius, 0.5f, 0.1f, 500.0f, "%.1f");

    if (ImGui::Button("Move to camera"))
        light.position = cameraPos;
    ImGui::SameLine();
    ImGui::TextDisabled("(%.2f, %.2f, %.2f)", cameraPos.x, cameraPos.y, cameraPos.z);

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.5f, 0.05f, 0.05f, 1.0f));
    const bool remove = ImGui::Button("Remove light");
    ImGui::PopStyleColor(3);

    ImGui::TreePop();
    return remove;
}

static void DrawShadowParamsDebug(LightShadowParams& shadow,
                                  std::string_view   farPlaneLabel,
                                  std::string_view   connectionNote,
                                  bool               showDirectionalGpuNote) {
    // Keep the debug UI bound directly to LightShadowParams so later shadow-resource
    // and sampling work can continue from the same runtime state.
    ImGui::Checkbox("Cast shadow", &shadow.castShadow);
    ImGui::DragFloat("Depth bias", &shadow.depthBias, 0.0001f, 0.0f, 1.0f, "%.5f");
    ImGui::DragFloat("Normal bias", &shadow.normalBias, 0.0005f, 0.0f, 10.0f, "%.4f");
    ImGui::DragFloat("Slope bias",  &shadow.slopeBias,  0.05f,   0.0f, 10.0f, "%.3f");

    int resolution[2] = {shadow.shadowMapWidth, shadow.shadowMapHeight};
    if (ImGui::DragInt2("Resolution", resolution, 4.0f, 16, 8192)) {
        shadow.shadowMapWidth  = resolution[0] < 16 ? 16 : resolution[0];
        shadow.shadowMapHeight = resolution[1] < 16 ? 16 : resolution[1];
    }

    ImGui::DragFloat("Near plane", &shadow.nearPlane, 0.01f, 0.001f, 1000.0f, "%.3f");
    ImGui::DragFloat(farPlaneLabel.data(), &shadow.farPlane, 0.05f, 0.01f, 10000.0f, "%.3f");
    if (shadow.nearPlane < 0.001f)
        shadow.nearPlane = 0.001f;
    if (shadow.farPlane <= shadow.nearPlane)
        shadow.farPlane = shadow.nearPlane + 0.01f;

    ImGui::SliderInt("PCF radius", &shadow.pcfRadius, 0, 4);
    if (shadow.pcfRadius < 0)
        shadow.pcfRadius = 0;

    const int kernelSize  = shadow.pcfRadius * 2 + 1;
    const int sampleCount = kernelSize * kernelSize;
    ImGui::Text("Derived PCF kernel: %dx%d (%d taps)", kernelSize, kernelSize, sampleCount);
    ImGui::TextDisabled("%s", connectionNote.data());
    if (showDirectionalGpuNote)
        ImGui::TextDisabled("Directional cast/depth/normal bias are already packed into the light UBO.");

    ImGui::TextDisabled("PCF enable/sample count are not separate engine fields; kernel is derived from radius only.");
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

        const bool lookModeActive = (m_mouse && m_mouse->IsCaptured());
        const ImGuiWindowFlags debugWindowFlags = lookModeActive ? ImGuiWindowFlags_NoMouseInputs : 0;

        if (ImGui::Begin("Renderer Debug", nullptr, debugWindowFlags)) {
            const RendererDebugStats& stats = m_renderer->GetDebugStats();
            const AssetCacheStats cacheStats = AssetImporter::GetCacheStats();
            LightEnvironment& liveLights = scene.GetLights();
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
            ImGui::Text("Queued casters: %u", stats.shadowCasterCount);
            if (stats.shadowCasterCountApproximate)
                ImGui::TextDisabled("Queued caster count comes from accepted RenderItem castShadow flags.");

            ImGui::SeparatorText("Active Lights");
            ImGui::Text("Directional lights: %d", liveLights.HasDirectionalLight() ? 1 : 0);
            ImGui::Text("Point lights      : %d", static_cast<int>(liveLights.GetPointLights().size()));
            ImGui::TextDisabled("Interactive: edits write back to the current scene lights and apply on the next frame.");

            if (liveLights.HasDirectionalLight()) {
                if (ImGui::TreeNode("Directional Light")) {
                    DrawDirectionalLightDebug(liveLights.GetDirectionalLight());
                    ImGui::TreePop();
                }
            } else {
                ImGui::TextDisabled("No directional light in the active scene.");
            }

            auto& pointLights = liveLights.GetPointLights();
            const glm::vec3 camPos = submission.camera ? submission.camera->GetPosition() : glm::vec3(0.0f);

            // Add button — placed before the list so it's always visible
            const bool atMax = static_cast<int>(pointLights.size()) >= kMaxPointLights;
            if (atMax) ImGui::BeginDisabled();
            if (ImGui::Button("+ Add point light")) {
                PointLight newLight;
                newLight.position  = camPos;
                newLight.color     = {1.0f, 1.0f, 1.0f};
                newLight.intensity = 3.0f;
                newLight.radius    = 20.0f;
                newLight.name      = "Light " + std::to_string(pointLights.size() + 1);
                liveLights.AddPointLight(newLight);
            }
            if (atMax) {
                ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::TextDisabled("(max %d reached)", kMaxPointLights);
            }

            if (pointLights.empty()) {
                ImGui::TextDisabled("No point lights in the active scene.");
            } else {
                int removeIndex = -1;
                for (std::size_t i = 0; i < pointLights.size(); ++i) {
                    ImGui::PushID(static_cast<int>(i));
                    if (DrawPointLightDebug(pointLights[i], static_cast<int>(i), camPos))
                        removeIndex = static_cast<int>(i);
                    ImGui::PopID();
                }
                if (removeIndex >= 0)
                    pointLights.erase(pointLights.begin() + removeIndex);
            }

            int directionalShadowLights = 0;
            if (liveLights.HasDirectionalLight() && liveLights.GetDirectionalLight().shadow.castShadow)
                directionalShadowLights = 1;

            int pointShadowLights = 0;
            for (const auto& point : liveLights.GetPointLights()) {
                if (point.shadow.castShadow)
                    ++pointShadowLights;
            }

            int spotShadowLights = 0;
            for (const auto& spot : liveLights.GetSpotLights()) {
                if (spot.shadow.castShadow)
                    ++spotShadowLights;
            }

            ImGui::SeparatorText("Shadow Debug");
            ImGui::Text("Directional shadow lights: %d", directionalShadowLights);
            ImGui::Text("Point shadow lights      : %d", pointShadowLights);
            ImGui::Text("Spot shadow lights       : %d", spotShadowLights);
            ImGui::TextDisabled("Interactive: edits write back to per-light shadow config and apply on the next frame.");
            ImGui::Spacing();
            ImGui::TextDisabled("Shadow Pass Stats");
            ImGui::Text("Shadow pass draws        : %u", stats.shadowPassObjectCount);
            ImGui::Text("Excluded (castShadow=no): %u", stats.shadowPassExcludedObjectCount);
            ImGui::Text("Receivers (receiveShadow): %u", stats.shadowReceiverCount);
            ImGui::Text("Queued casters           : %u", stats.shadowCasterCount);
            if (stats.shadowPassDataAvailable && stats.shadowPassObjectCount == 0)
                ImGui::TextDisabled("Live directional shadow pass ran, but no submitted object reached the depth draw path.");
            if (!stats.shadowPassDataAvailable)
                ImGui::TextDisabled("Directional shadow pass is inactive for this frame, so pass-only counters stay at 0.");

            ImGui::Spacing();
            ImGui::TextDisabled("Directional Frustum");
            if (stats.directionalShadowFrustum.available) {
                ImGui::Text("Focus center (world)     : %.2f, %.2f, %.2f",
                            stats.directionalShadowFrustum.focusCenterX,
                            stats.directionalShadowFrustum.focusCenterY,
                            stats.directionalShadowFrustum.focusCenterZ);
                ImGui::Text("Light direction          : %.2f, %.2f, %.2f",
                            stats.directionalShadowFrustum.lightDirectionX,
                            stats.directionalShadowFrustum.lightDirectionY,
                            stats.directionalShadowFrustum.lightDirectionZ);
                ImGui::Text("Ortho radius             : %.2f", stats.directionalShadowFrustum.orthoRadius);
                ImGui::Text("Near / far clip          : %.2f / %.2f",
                            stats.directionalShadowFrustum.nearPlane,
                            stats.directionalShadowFrustum.farPlane);
                ImGui::TextDisabled("Bounds come from the current coarse caster-fit helper.");
            } else {
                ImGui::TextDisabled("Directional frustum info is unavailable for the current frame.");
            }

            ImGui::Spacing();
            ImGui::TextDisabled("Cascade Shadow Map Preview");
            if (stats.shadowMapPreviewAvailable) {
                ImGui::Text("Resolution per cascade   : %u x %u", stats.shadowMapWidth, stats.shadowMapHeight);

                const float previewWidth  = 150.0f;
                const float previewHeight = stats.shadowMapWidth > 0
                    ? (previewWidth * static_cast<float>(stats.shadowMapHeight) / static_cast<float>(stats.shadowMapWidth))
                    : previewWidth;

                // Lay the 4 cascade previews out in a 2x2 grid with a caption
                // above each image showing its view-space far-edge distance.
                for (size_t i = 0; i < stats.cascadePreviewTextureIds.size(); ++i) {
                    const uint32_t texId = stats.cascadePreviewTextureIds[i];
                    const float    splitFar = stats.cascadeSplitDistances[i];
                    const float    splitNear = (i == 0) ? 0.0f : stats.cascadeSplitDistances[i - 1];

                    ImGui::BeginGroup();
                    ImGui::Text("Cascade %zu: [%.1f, %.1f]", i, splitNear, splitFar);
                    if (texId != 0) {
                        ImGui::Image(
                            reinterpret_cast<ImTextureID>(static_cast<intptr_t>(texId)),
                            ImVec2(previewWidth, previewHeight),
                            ImVec2(0.0f, 1.0f),
                            ImVec2(1.0f, 0.0f));
                    } else {
                        ImGui::Dummy(ImVec2(previewWidth, previewHeight));
                        ImGui::TextDisabled("no view");
                    }
                    ImGui::EndGroup();

                    // Two previews per row.
                    if ((i % 2) == 0)
                        ImGui::SameLine();
                }
                ImGui::TextDisabled("Previews are 2D views aliasing the depth array's cascade layers.");
            } else {
                ImGui::TextDisabled("Cascade shadow map preview is unavailable for the current frame.");
            }

            ImGui::Spacing();
            ImGui::TextDisabled("Shadow Controls");
            if (liveLights.HasDirectionalLight()) {
                if (ImGui::TreeNode("Directional Shadow")) {
                    DrawShadowParamsDebug(
                        liveLights.GetDirectionalLight().shadow,
                        "Far / extent",
                        "Resolution is live for directional shadow-map generation; near/far and PCF radius stay on LightShadowParams for future shadow integration.",
                        true);
                    ImGui::TreePop();
                }
            } else {
                ImGui::TextDisabled("No directional shadow config in the active scene.");
            }

            if (pointLights.empty()) {
                ImGui::TextDisabled("No point-light shadow config in the active scene.");
            } else {
                for (std::size_t i = 0; i < pointLights.size(); ++i) {
                    const std::string label = "Point Shadow " + std::to_string(i + 1);
                    ImGui::PushID(static_cast<int>(i));
                    if (ImGui::TreeNode(label.c_str())) {
                        ImGui::Text("Name      : %s", pointLights[i].name.empty() ? "(unnamed)" : pointLights[i].name.c_str());
                        DrawShadowParamsDebug(
                            pointLights[i].shadow,
                            "Far plane",
                            "Point-light shadow params stay in scene state until Person 7/8 land the point shadow pass and sampling hooks.",
                            false);
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
            }

            if (!liveLights.GetSpotLights().empty())
                ImGui::TextDisabled("Spot-light shadow UI is not added in this phase; shadow-enabled count is shown above.");

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
