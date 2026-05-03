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

    glm::vec3 rotAxis;
    float rotSpeed;
    float baseScale;
};

class JapanScene : public Scene {
public:
    bool Setup();
    void OnUpdate(float deltaTime, IInputProvider &input) override;

private:
    // ── Castle model ──────────────────────────────────────────────────────────
    ModelData m_castleModel;
    std::shared_ptr<Material> m_castleBaseMaterial;
    std::vector<std::unique_ptr<MaterialInstance> > m_castleMatInstances;

    std::unique_ptr<MeshBuffer> m_petalMesh;
    std::vector<Petal> m_petals;

    // ── Sekiro player model ───────────────────────────────────────────────────
    ModelData                             m_sekiroModel;
    std::shared_ptr<Material>             m_sekiroMaterial;
    std::vector<std::unique_ptr<MaterialInstance>> m_sekiroMatInstances;
    std::vector<size_t>                   m_sekiroSubMeshIndices;

    // ── Player state ──────────────────────────────────────────────────────────
    glm::vec3 m_playerPosition = {8.74f, 1.20f, -8.90f};
    size_t    m_playerCubeIdx  = (size_t)-1;

    // ── Wind System ───────────────────────────────────────────────────────────
    float m_windTimer = 0.0f;
    float m_windInterval = 5.0f;     // time between gusts
    float m_windDuration = 1.5f;     // how long gust lasts
    bool  m_windActive = false;

    glm::vec3 m_windDir = {1.0f, 0.0f, 0.0f};
    float m_windStrength = 5.0f;
};