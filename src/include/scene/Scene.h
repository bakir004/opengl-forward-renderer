#pragma once

#include "core/IInputProvider.h"
#include "core/Camera.h"
#include "core/CameraController.h"
#include "scene/RenderItem.h"
#include "scene/LightEnvironment.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <memory>

class IInputProvider;
struct FrameSubmission;
class Skybox;
struct ReflectionProbe;

/// Base class for all scenes.
///
/// A scene owns its camera and all renderable objects. Subclasses declare content
/// in their own setup method (AddObject, SetCamera, SetClearColor) and override
/// OnUpdate for per-frame logic. Application::Run(Scene&) drives the loop.
class Scene {
public:
    Scene()
        : m_standardCameraController(m_camera) {
    }

    virtual ~Scene() = default;

    /// Returns the name of the scene for display in UI.
    virtual const std::string &GetName() const { return m_sceneName; }

    /// Called once per frame. Override to handle input and update scene state.
    virtual void OnUpdate(float deltaTime, IInputProvider &input) {
    }

    /// Sets whether the skybox should be rendered.
    void SetSkyboxVisible(bool visible) { m_skyboxVisible = visible; }

    /// Returns whether the skybox is currently set to be rendered.
    bool IsSkyboxVisible() const { return m_skyboxVisible; }

    /// Called during the ImGui frame. Override to draw custom debug UI for the scene.
    virtual void OnImGuiRender() {
    }

protected:
    /// Replaces the scene camera.
    void SetCamera(Camera camera);

    /// Sets the background clear colour.
    void SetClearColor(glm::vec4 color);

    /// Sets the scene skybox.
    void SetSkybox(std::shared_ptr<Skybox> skybox);

    /// Returns the skybox
    [[nodiscard]] Skybox *GetSkybox() const { return m_skybox.get(); }
    /// Sets the active reflection probe used by PBR/IBL shaders.
    void SetReflectionProbe(std::shared_ptr<ReflectionProbe> probe);

    /// Returns the active reflection probe so scenes can update intensity or influence settings.
    [[nodiscard]] std::shared_ptr<ReflectionProbe> GetReflectionProbe() const { return m_reflectionProbe; }

    /// Sets the active probe intensity when a probe exists.
    void SetIblIntensity(float intensity);

    /// Adds a render item to the scene.
    /// @return Index that can be passed to GetObject() for per-frame updates.
    size_t AddObject(RenderItem item);

    /// Returns a mutable reference to the object at the given index.
    RenderItem &GetObject(size_t index);

    /// Returns a mutable reference to the scene camera.
    Camera &GetCamera();

    /// Returns mutable per-scene lights that will be copied into FrameSubmission.
    LightEnvironment &GetLights();

    /// Returns read-only per-scene lights.
    const LightEnvironment &GetLights() const;

    /// Convenience helper for ambient lighting setup.
    void SetAmbientLight(const glm::vec3 &color, float intensity);

    /// Returns the base movement speed configured for the current camera mode (in meters/sec).
    float GetCurrentCameraSpeed() const;

    /// Sets the scene name for display in the UI.
    void SetSceneName(const std::string &name) { m_sceneName = name; }

    // -- Movement properties --
    float m_cameraFreeFlySpeed = 4.0f;
    float m_cameraFirstPersonSpeed = 3.0f;
    float m_cameraThirdPersonSpeed = 3.0f;
    float m_cameraOrbitRadius = 5.0f;
    float m_lastEffectiveSpeed = 0.0f; // Tracks exact speed including sprint

    /// Shared standard camera logic (WASD, TAB, F1-F3, mouselook, sprint/zoom).
    /// Safe to call each frame in OnUpdate by subclasses.
    void UpdateStandardCameraAndPlayer(float deltaTime, IInputProvider &input,
                                       glm::vec3 &playerPos, glm::vec3 &outMoveDirXZ,
                                       float orbitTargetYOffset = 0.0f);

    void SetFirstPersonEyeHeight(float height) { m_standardCameraController.SetFirstPersonEyeHeight(height); }

private:
    /// Called by Application each frame before rendering.
    /// Updates camera aspect ratio from the current framebuffer, then calls OnUpdate.
    void InternalUpdate(float deltaTime, IInputProvider &input, int fbWidth, int fbHeight);

    /// Fills a FrameSubmission from current scene state. Called by Application.
    void BuildSubmission(FrameSubmission &out) const;

    Camera m_camera;
    StandardSceneCameraController m_standardCameraController;
    std::shared_ptr<Skybox> m_skybox;
    std::shared_ptr<ReflectionProbe> m_reflectionProbe;
    bool                 m_skyboxVisible = true;
    LightEnvironment     m_lights;
    std::vector<RenderItem> m_objects;
    glm::vec4 m_clearColor = {0.08f, 0.09f, 0.12f, 1.0f};
    std::string m_sceneName = "Untitled Scene";
    mutable bool m_reportedInvalidLights = false;

    friend class Application;
    friend class RendererUI;
};
