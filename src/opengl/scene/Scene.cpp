#include "scene/Scene.h"
#include "scene/FrameSubmission.h"

void Scene::SetCamera(Camera camera) {
    m_camera = std::move(camera);
}

void Scene::SetClearColor(glm::vec4 color) {
    m_clearColor = color;
}

size_t Scene::AddObject(RenderItem item) {
    m_objects.push_back(std::move(item));
    return m_objects.size() - 1;
}

RenderItem& Scene::GetObject(size_t index) {
    return m_objects[index];
}

Camera& Scene::GetCamera() {
    return m_camera;
}

void Scene::InternalUpdate(float deltaTime, KeyboardInput& input, MouseInput& mouse, int fbWidth, int fbHeight) {
    m_camera.OnResize(fbWidth, fbHeight);
    OnUpdate(deltaTime, input, mouse);
}

void Scene::BuildSubmission(FrameSubmission& out) const {
    out.camera                   = &m_camera;
    out.clearInfo.clearColor     = m_clearColor;
    out.clearInfo.clearFlags     = ClearFlags::ColorDepth;
    out.objects                  = m_objects;
}
