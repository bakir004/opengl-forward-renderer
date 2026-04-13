#pragma once

#include "scene/Scene.h"
#include "core/Material.h"
#include "assets/AssetImporter.h"
#include "assets/ModelData.h"
#include <memory>
#include <vector>

/// Neon city night scene loaded as a multi-material model.
/// One RenderItem and one MaterialInstance are created per submesh so each
/// part of the city renders with its own diffuse texture.
class NeonCityScene : public Scene
{
public:
    bool Setup();
    void OnUpdate(float deltaTime, KeyboardInput& input, MouseInput& mouse) override;

private:
    ModelData                                    m_cityModel;
    std::shared_ptr<Material>                    m_cityBaseMaterial;
    std::vector<std::unique_ptr<MaterialInstance>> m_cityMatInstances;

    glm::vec3 m_playerPosition = {0.0f, 1.0f, 8.0f};
};
