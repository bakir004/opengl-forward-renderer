#pragma once

#include "scene/Scene.h"
#include "assets/AssetImporter.h"
#include "core/MeshBuffer.h"
#include "core/Material.h"
#include "core/Material.h"
#include <glm/glm.hpp>
#include <memory>
#include <vector>

struct Petal {
    size_t idx;
    glm::vec3 pos;
    float speed;
    float swayPhase;
};

class JapanScene : public Scene {
public:
    bool Setup();
    void OnUpdate(float deltaTime, KeyboardInput &input, MouseInput &mouse) override;

private:
    // ── Castle model ──────────────────────────────────────────────────────────
    ModelData m_castleModel;
    std::shared_ptr<Material> m_castleBaseMaterial;
    std::vector<std::unique_ptr<MaterialInstance> > m_castleMatInstances;

    std::unique_ptr<MeshBuffer> m_petalMesh;
    std::vector<Petal> m_petals;

    // ── Player state ──────────────────────────────────────────────────────────
    glm::vec3 m_playerPosition = {0.0f, 0.0f, 0.0f};
    int m_playerCubeIdx = -1;
};