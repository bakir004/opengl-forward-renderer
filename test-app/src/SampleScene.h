#pragma once

#include "scene/Scene.h"
#include "core/MeshBuffer.h"
#include "core/ShaderProgram.h"
#include <memory>

/// Demo scene: triangle, quad, several cubes, and a sphere.
/// Camera and all render items are declared in Setup().
/// Per-frame input handling lives in OnUpdate().
class SampleScene : public Scene {
public:
    /// Uploads geometry and sets up the scene. Must be called after Initialize().
    /// @return false if a shader fails to compile/link.
    bool Setup();

    void OnUpdate(float deltaTime, KeyboardInput& input, MouseInput& mouse) override;

private:
    std::unique_ptr<ShaderProgram> m_shader;
    std::unique_ptr<MeshBuffer>    m_triangle;
    std::unique_ptr<MeshBuffer>    m_quad;
    std::unique_ptr<MeshBuffer>    m_cube;
    std::unique_ptr<MeshBuffer>    m_sphere;

    size_t    m_playerCubeIdx  = 0;
    glm::vec3 m_playerPosition = {0.4f, 0.0f, -3.0f};
};
