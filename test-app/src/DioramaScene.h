#pragma once

#include "scene/Scene.h"
#include "core/MeshBuffer.h"
#include "core/ShaderProgram.h"
#include "core/Material.h"
#include "assets/AssetImporter.h"
#include <memory>

class DioramaScene : public Scene {
public:
    bool Setup();
    void OnUpdate(float deltaTime, KeyboardInput& input, MouseInput& mouse) override;

private:
    std::unique_ptr<ShaderProgram> m_shader;

    // Imported models
    std::shared_ptr<MeshBuffer>       m_duck;
    std::shared_ptr<Material>         m_duckMaterial;
    std::unique_ptr<MaterialInstance> m_duckMatInst;

    std::shared_ptr<MeshBuffer>       m_lantern;
    std::shared_ptr<Material>         m_lanternMaterial;
    std::unique_ptr<MaterialInstance> m_lanternMatInst;

    std::shared_ptr<MeshBuffer>       m_indoorPlant;
    std::shared_ptr<Material>         m_indoorPlantMaterial;
    std::unique_ptr<MaterialInstance> m_indoorPlantMatInst;

    std::shared_ptr<MeshBuffer>       m_house;
    std::shared_ptr<Material>         m_houseMaterial;
    std::unique_ptr<MaterialInstance> m_houseMatInst;

    // Geometry
    std::unique_ptr<MeshBuffer> m_colorQuad;
    std::shared_ptr<MeshBuffer>       m_pool;
    std::shared_ptr<Material>         m_poolMaterial;
    std::unique_ptr<MaterialInstance> m_poolMatInst;

    std::shared_ptr<MeshBuffer>       m_grass;
    std::shared_ptr<Material>         m_grassMaterial;
    std::unique_ptr<MaterialInstance> m_grassMatInst;

    std::shared_ptr<MeshBuffer>       m_tree;
    std::shared_ptr<Material>         m_treeMaterial;
    std::unique_ptr<MaterialInstance> m_treeMatInst;

    std::shared_ptr<MeshBuffer>       m_bench;
    std::shared_ptr<Material>         m_benchMaterial;
    std::unique_ptr<MaterialInstance> m_benchMatInst;


    std::unique_ptr<MeshBuffer> m_wallQuad;

    size_t m_duckInsideIdx = 0;
    float m_duckInsideAngle = 0.0f;

    std::unique_ptr<MeshBuffer> m_fireflyMesh;
    size_t m_fireflyIdx = (size_t)-1;
    float m_fireflyTimer = 0.0f;

    size_t m_playerCubeIdx = 0;
    glm::vec3 m_playerPosition = {-2.0f, -0.6f, 2.0f};
    float m_cameraSpeed = 3.0f;
};
