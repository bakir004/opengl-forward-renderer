#pragma once

#include "core/Camera.h"
#include "scene/RenderItem.h"
#include "scene/LightEnvironment.h"
#include <glm/glm.hpp>
#include <vector>

class KeyboardInput;
class MouseInput;
struct FrameSubmission;
class DirectionalLight;
class PointLight;
class SpotLight;

/// Base class for all scenes.
///
/// A scene owns its camera and all renderable objects. Subclasses declare content
/// in their own setup method (AddObject, SetCamera, SetClearColor) and override
/// OnUpdate for per-frame logic. Application::Run(Scene&) drives the loop.
class Scene
{
public:
    virtual ~Scene() = default;

    /// Called once per frame. Override to handle input and update scene state.
    virtual void OnUpdate(float deltaTime, KeyboardInput &input, MouseInput &mouse) {}

protected:
    /// Replaces the scene camera.
    void SetCamera(Camera camera);

    /// Sets the background clear colour.
    void SetClearColor(glm::vec4 color);

    /// Adds a render item to the scene.
    /// @return Index that can be passed to GetObject() for per-frame updates.
    size_t AddObject(RenderItem item);

    /// Returns a mutable reference to the object at the given index.
    RenderItem &GetObject(size_t index);

    /// Returns a mutable reference to the scene camera.
    Camera &GetCamera();

    /// Returns the base movement speed configured for the current camera mode (in meters/sec).
    float GetCurrentCameraSpeed() const;

    // Light management
    /// Sets the scene's directional light. Only one directional light is supported per frame.
    void SetDirectionalLight(const DirectionalLight &light);

    /// Adds a point light to the scene. Up to kMaxPointLights lights are supported.
    void AddPointLight(const PointLight &light);

    /// Adds a spot light to the scene. Up to kMaxSpotLights lights are supported.
    void AddSpotLight(const SpotLight &light);

    /// Clears all lights from the scene.
    void ClearLights();

    /// Returns a mutable reference to the scene's light environment.
    LightEnvironment &GetLightEnvironment();

    /// Returns a const reference to the scene's light environment.
    const LightEnvironment &GetLightEnvironment() const;

    // -- Movement properties --
    float m_cameraFreeFlySpeed = 4.0f;
    float m_cameraFirstPersonSpeed = 3.0f;
    float m_cameraThirdPersonSpeed = 3.0f;
    float m_cameraOrbitRadius = 5.0f;
    float m_lastEffectiveSpeed = 0.0f; // Tracks exact speed including sprint

    /// Shared standard camera logic (WASD, TAB, F1-F3, mouselook, sprint/zoom).
    /// Safe to call each frame in OnUpdate by subclasses.
    void UpdateStandardCameraAndPlayer(float deltaTime, KeyboardInput &input, MouseInput &mouse,
                                       glm::vec3 &playerPos, glm::vec3 &outMoveDirXZ,
                                       float orbitTargetYOffset = 0.0f);

private:
    /// Called by Application each frame before rendering.
    /// Updates camera aspect ratio from the current framebuffer, then calls OnUpdate.
    void InternalUpdate(float deltaTime, KeyboardInput &input, MouseInput &mouse, int fbWidth, int fbHeight);

    /// Fills a FrameSubmission from current scene state. Called by Application.
    void BuildSubmission(FrameSubmission &out) const;

    Camera m_camera;
    std::vector<RenderItem> m_objects;
    glm::vec4 m_clearColor = {0.08f, 0.09f, 0.12f, 1.0f};
    LightEnvironment m_lights;

    friend class Application;
};
