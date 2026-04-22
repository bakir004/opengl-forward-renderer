#pragma once

#include "scene/Scene.h"
#include "core/MeshBuffer.h"
#include "core/ShaderProgram.h"
#include "core/Material.h"
#include "assets/AssetImporter.h"
#include <memory>

/// Demo scene: triangle, quad, several cubes, a pyramid, and a sphere.
/// Camera and all render items are declared in Setup().
/// Per-frame input handling lives in OnUpdate().
class SampleScene : public Scene
{
public:
    /// Uploads geometry and sets up the scene. Must be called after Initialize().
    /// @return false if a shader fails to compile/link.
    bool Setup();

    void OnUpdate(float deltaTime, KeyboardInput &input, MouseInput &mouse) override;

private:
    std::unique_ptr<ShaderProgram> m_shader;

    // Imported models
    std::shared_ptr<MeshBuffer> m_avocado;
    std::shared_ptr<Material> m_avocadoMaterial;
    std::unique_ptr<MaterialInstance> m_avocadoMatInst;

    std::shared_ptr<MeshBuffer> m_duck;
    std::shared_ptr<Material> m_duckMaterial;
    std::unique_ptr<MaterialInstance> m_duckMatInst;

    std::shared_ptr<MeshBuffer> m_lantern;
    std::shared_ptr<Material> m_lanternMaterial;
    std::unique_ptr<MaterialInstance> m_lanternMatInst;

    // --- ADDED --- Ground plane material (for shadow receiving)
    std::shared_ptr<Material> m_groundMaterial;
    std::unique_ptr<MaterialInstance> m_groundMatInst;

    // Geometry (uploaded to GPU once, shared across multiple RenderItems)
    std::unique_ptr<MeshBuffer> m_triangle;
    std::unique_ptr<MeshBuffer> m_quad;
    std::unique_ptr<MeshBuffer> m_colorQuad;
    std::unique_ptr<MeshBuffer> m_rainbowCube;
    std::unique_ptr<MeshBuffer> m_solidCube;
    std::unique_ptr<MeshBuffer> m_pyramid;
    std::unique_ptr<MeshBuffer> m_sphere;

    size_t m_playerCubeIdx = 0;
    size_t m_pyramidIdx = 0;
    float m_pyramidRotY = 0.0f;
    glm::vec3 m_playerPosition = {-2.0f, 0.5f, 2.0f};

    // --- ADDED --- Light animation tracking
    float m_lightAnimTime = 0.0f;

    // --- ADDED --- Duck floating animation
    float m_duckFloatTime = 0.0f;

    // --- ADDED --- Lantern light control
    size_t m_lanternLightIdx = 0;
    bool m_lanternLightEnabled = true;

    // --- ADDED --- Additional toggleable light
    size_t m_posLightIdx = 0;
    bool m_posLightEnabled = true;
};
