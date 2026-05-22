#pragma once

#include "scene/Scene.h"
#include <memory>
#include <vector>

class Mesh;
class MeshBuffer;
class Material;
class MaterialInstance;

class IblValidationScene : public Scene
{
public:
    bool Setup();
    void OnUpdate(float deltaTime, IInputProvider& input) override;
    void OnImGuiRender() override;

private:
    std::shared_ptr<Mesh> m_sphereMesh;
    std::shared_ptr<Mesh> m_planeMesh;
    std::shared_ptr<Material> m_gridBaseMaterial;
    std::shared_ptr<Material> m_planeMaterial;
    std::vector<std::unique_ptr<MaterialInstance>> m_materials;
    
    std::shared_ptr<MeshBuffer> m_bench;
    std::shared_ptr<Material> m_benchMat;

    glm::vec3 m_cameraAnchor{0.0f, 1.0f, 0.0f};

    int m_lightingMode = 1;
    int m_skyboxMode = 0;
    float m_dirLightIntensity = 4.0f;
    std::shared_ptr<Skybox> m_mountainsSkybox;
    std::shared_ptr<Skybox> m_moodySkybox;
    std::shared_ptr<Skybox> m_neutralSkybox;
    std::shared_ptr<Skybox> m_outdoorSkybox;
    std::shared_ptr<ReflectionProbe> m_mountainsProbe;
    std::shared_ptr<ReflectionProbe> m_moodyProbe;
    std::shared_ptr<ReflectionProbe> m_neutralProbe;
    std::shared_ptr<ReflectionProbe> m_outdoorProbe;
};
