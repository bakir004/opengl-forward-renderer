#pragma once

#include "scene/Scene.h"
#include "core/Material.h"
#include "assets/AssetImporter.h"
#include "assets/ModelData.h"
#include <memory>
#include <vector>

class MeshBuffer;
class MaterialInstance;

class NormalMapScene : public Scene
{
public:
    bool Setup();
    void OnUpdate(float deltaTime, IInputProvider& input) override;

private:
    // --- Bench ---
    std::shared_ptr<MeshBuffer>       m_bench;
    std::shared_ptr<Material>         m_benchMat;

    // --- Avocado (with full PBR textures incl. normal map) ---
    std::shared_ptr<MeshBuffer>       m_avocado;
    std::shared_ptr<Material>         m_avocadoMat;

    // --- Lantern (with full PBR textures incl. normal map + emissive) ---
    std::shared_ptr<MeshBuffer>       m_lantern;
    std::shared_ptr<Material>         m_lanternMat;

    // --- Wooden Kitchen (multi-material GLTF, 29 materials / 98 submeshes) ---
    ModelData                         m_kitchen;
    std::vector<std::shared_ptr<Material>> m_kitchenMats;


    // All MaterialInstance instances kept alive for the scene lifetime
    std::vector<std::unique_ptr<MaterialInstance>> m_instances;

    // Camera anchor for orbit mode
    glm::vec3 m_cameraAnchor{ 0.0f, 0.6f, 0.0f };
};
