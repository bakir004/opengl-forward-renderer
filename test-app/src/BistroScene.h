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
    void OnImGuiRender() override;

private:
    ModelData m_bistroInteriorModel;
    ModelData m_bistroExteriorModel;
    std::shared_ptr<Material> m_bistroBaseMaterial;
    std::vector<std::unique_ptr<MaterialInstance>> m_interiorMaterials;
    std::vector<std::unique_ptr<MaterialInstance>> m_exteriorMaterials;
    int m_skyboxMode = 0;
    std::shared_ptr<Skybox> m_bistroSkybox;
    std::shared_ptr<Skybox> m_mountainsSkybox;
    std::shared_ptr<Skybox> m_moodySkybox;
    std::shared_ptr<Skybox> m_neutralSkybox;
    std::shared_ptr<Skybox> m_outdoorSkybox;
    std::shared_ptr<ReflectionProbe> m_bistroProbe;
    std::shared_ptr<ReflectionProbe> m_mountainsProbe;
    std::shared_ptr<ReflectionProbe> m_moodyProbe;
    std::shared_ptr<ReflectionProbe> m_neutralProbe;
    std::shared_ptr<ReflectionProbe> m_outdoorProbe;
    glm::vec3 m_cameraAnchor{0.0f, 1.7f, 0.0f};
};