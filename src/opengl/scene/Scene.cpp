#include "scene/Scene.h"
#include "scene/FrameSubmission.h"
#include "scene/LightUtils.h"
#include "core/IInputProvider.h"
#include "scene/ReflectionProbe.h"
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

void Scene::SetCamera(Camera camera) {
    m_camera = std::move(camera);
}

void Scene::SetClearColor(glm::vec4 color) {
    m_clearColor = color;
}

void Scene::SetSkybox(std::shared_ptr<Skybox> skybox) {
    m_skybox = std::move(skybox);
}

void Scene::SetReflectionProbe(std::shared_ptr<ReflectionProbe> probe) {
    m_reflectionProbe = std::move(probe);
}

void Scene::SetIblIntensity(float intensity) {
    if (m_reflectionProbe)
        m_reflectionProbe->intensity = intensity;
}

size_t Scene::AddObject(RenderItem item) {
    if (!item.material && !item.shader)
        spdlog::warn("[Scene] AddObject: RenderItem has no material or shader — will render with error shader");
    m_objects.push_back(std::move(item));
    return m_objects.size() - 1;
}

RenderItem& Scene::GetObject(size_t index) {
    return m_objects[index];
}

Camera& Scene::GetCamera() {
    return m_camera;
}

LightEnvironment& Scene::GetLights() {
    return m_lights;
}

const LightEnvironment& Scene::GetLights() const {
    return m_lights;
}

void Scene::SetAmbientLight(const glm::vec3& color, float intensity) {
    m_lights.ambientColor = color;
    m_lights.ambientIntensity = intensity;
}

float Scene::GetCurrentCameraSpeed() const {
    return m_lastEffectiveSpeed;
}

void Scene::InternalUpdate(float deltaTime, IInputProvider& input, int fbWidth, int fbHeight) {
    m_camera.OnResize(fbWidth, fbHeight);
    OnUpdate(deltaTime, input);
}

void Scene::BuildSubmission(FrameSubmission& out) const {
    out.camera                   = &m_camera;
    out.skybox                   = m_skyboxVisible ? m_skybox.get() : nullptr;
    out.activeReflectionProbe    = m_reflectionProbe.get();
    out.clearInfo.clearColor     = m_clearColor;
    out.clearInfo.clearFlags     = ClearFlags::ColorDepth;
    out.lights                   = m_lights;
    out.objects                  = m_objects;

    const bool lightsValid = LightUtils::Validate(out.lights);
    if (!lightsValid && !m_reportedInvalidLights) {
        spdlog::warn("[Scene] BuildSubmission: light setup has validation warnings");
        m_reportedInvalidLights = true;
    } else if (lightsValid && m_reportedInvalidLights) {
        m_reportedInvalidLights = false;
    }
}

void Scene::UpdateStandardCameraAndPlayer(float deltaTime, IInputProvider& input, 
                                          glm::vec3& playerPos, glm::vec3& outMoveDirXZ, 
                                          float orbitTargetYOffset) 
{
    m_standardCameraController.Update(deltaTime,
                                      input,
                                      playerPos,
                                      outMoveDirXZ,
                                      orbitTargetYOffset,
                                      m_cameraFreeFlySpeed,
                                      m_cameraFirstPersonSpeed,
                                      m_cameraThirdPersonSpeed,
                                      m_cameraOrbitRadius,
                                      m_lastEffectiveSpeed);
}
