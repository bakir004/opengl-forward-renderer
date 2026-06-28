#pragma once

#include "scene/Scene.h"

#include <memory>
#include <vector>

class Material;
class MaterialInstance;
class Mesh;
class ShaderProgram;

enum class CapturePresetKind
{
    Baseline,
    DenseGrid,
    MaterialSweep,
    CullingTest,
};

class CapturePresetScene : public Scene
{
public:
    explicit CapturePresetScene(CapturePresetKind kind);

    bool Setup();
    void OnUpdate(float deltaTime, IInputProvider& input) override;

private:
    CapturePresetKind m_kind;
    std::shared_ptr<ShaderProgram> m_meshShader;
    std::shared_ptr<Mesh> m_cubeMesh;
    std::shared_ptr<Mesh> m_sphereMesh;
    std::shared_ptr<Mesh> m_planeMesh;
    std::shared_ptr<Material> m_baseMaterial;
    std::vector<std::unique_ptr<MaterialInstance>> m_materials;
    glm::vec3 m_cameraAnchor{0.0f, 1.4f, 0.0f};

    MaterialInstance* CreateMaterial(const char* name,
                                     glm::vec3 albedo,
                                     float metallic,
                                     float roughness,
                                     glm::vec3 emissive = glm::vec3(0.0f),
                                     float emissiveStrength = 1.0f);

    void AddMeshItem(const std::shared_ptr<Mesh>& mesh,
                     const MaterialInstance* material,
                     glm::vec3 translation,
                     glm::vec3 scale,
                     glm::vec3 rotationDegrees = glm::vec3(0.0f),
                     bool castShadow = true,
                     bool receiveShadow = true);

    void AddGroundAndBackdrop(float groundExtent, const MaterialInstance* material);
    void ConfigureSharedLighting(bool shadowsEnabled);
    void SetupBaselinePreset();
    void SetupDenseGridPreset();
    void SetupMaterialSweepPreset();
    void SetupCullingPreset();
};
