#pragma once

#include "scene/Scene.h"
#include "assets/AssetImporter.h"
#include "assets/ModelData.h"
#include "core/Material.h"
#include <memory>
#include <vector>

class Material;
class MaterialInstance;
class Skybox;
struct ReflectionProbe;

class BistroScene : public Scene
{
public:
    bool Setup();
    void OnUpdate(float deltaTime, IInputProvider& input) override;

private:
    ModelData m_bistroInteriorModel;
    ModelData m_bistroExteriorModel;
    std::shared_ptr<Material> m_bistroBaseMaterial;
    std::vector<std::unique_ptr<MaterialInstance>> m_interiorMaterials;
    std::vector<std::unique_ptr<MaterialInstance>> m_exteriorMaterials;
    std::shared_ptr<Skybox> m_skybox;
    std::shared_ptr<ReflectionProbe> m_probe;
    glm::vec3 m_cameraAnchor{0.0f, 1.7f, 0.0f};
};