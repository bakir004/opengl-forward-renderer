#pragma once

#include "scene/Scene.h"
#include <memory>
#include <vector>

class Mesh;
class Material;
class MaterialInstance;

class PbrValidationScene : public Scene
{
public:
    bool Setup();
    void OnUpdate(float deltaTime, IInputProvider& input) override;

private:
    std::shared_ptr<Mesh> m_sphereMesh;
    std::shared_ptr<Mesh> m_planeMesh;
    std::shared_ptr<Material> m_gridBaseMaterial;
    std::shared_ptr<Material> m_planeMaterial;
    std::vector<std::unique_ptr<MaterialInstance>> m_materials;
    glm::vec3 m_cameraAnchor{0.0f, 1.0f, 0.0f};
};
