#pragma once

#include "scene/Scene.h"
#include "assets/AssetImporter.h"
#include "core/MeshBuffer.h"
#include "core/Material.h"
#include <glm/glm.hpp>
#include <memory>
#include <vector>

/**
 * @struct Petal
 * @brief Represents a single cherry blossom petal in the scene's particle-like system.
 * 
 * Each petal has its own position, speed, and animation state to create a 
 * natural-looking falling effect.
 */
struct Petal {
    size_t idx;        ///< The index/ID of the RenderItem in the Scene
    glm::vec3 pos;     ///< Current world-space position
    float speed;       ///< Vertical falling speed
    float swayPhase;   ///< Current time-based phase for sinusoidal sway and rotation

    glm::vec3 rotAxis; ///< Randomly assigned axis for the petal's rotation
    float rotSpeed;    ///< Speed of rotation around the assigned axis
    float baseScale;   ///< Initial uniform scale before flutter is applied
};

/**
 * @class JapanScene
 * @brief A Japanese-themed demonstration scene.
 * 
 * This scene features:
 * - A high-detail Japanese Castle model (GLTF)
 * - A CPU-based particle system for falling cherry blossom petals
 * - A dynamic wind system that affects petal behavior
 * - A playable Sekiro model that syncs with the camera
 * - Custom lighting accents
 */
class JapanScene : public Scene {
public:
    /**
     * @brief Initializes the scene assets, lighting, and objects.
     * @return true if all critical assets loaded successfully.
     */
    bool Setup();

    /**
     * @brief Updates the scene logic every frame.
     * @param deltaTime Time elapsed since the last frame in seconds.
     * @param input Provider for user keyboard/mouse input.
     */
    void OnUpdate(float deltaTime, IInputProvider &input) override;

private:
    // ── Castle model ──────────────────────────────────────────────────────────
    ModelData m_castleModel;                                  ///< Raw model data for the castle
    std::shared_ptr<Material> m_castleBaseMaterial;           ///< Shared shader/uniform template for the castle
    std::vector<std::unique_ptr<MaterialInstance>> m_castleMatInstances; ///< Instances per GLTF material

    // ── Cherry Blossom System ────────────────────────────────────────────────
    std::unique_ptr<MeshBuffer> m_primitivePetalMesh;         ///< Primitive mesh used for petals
    std::shared_ptr<Material> m_petalBaseMaterial;            ///< Base material for petals
    std::vector<std::unique_ptr<MaterialInstance>> m_petalMatInstances; ///< Instances for petal materials
    std::vector<Petal> m_petals;                              ///< Logic state for each individual petal

    // ── Moon ──────────────────────────────────────────────────────────────────
    ModelData m_moonModel;                                    ///< Raw model data for the moon
    std::shared_ptr<Material> m_moonBaseMaterial;             ///< Base material for the moon
    std::vector<std::unique_ptr<MaterialInstance>> m_moonMatInstances; ///< Instances for moon materials

    // ── Sekiro player model ───────────────────────────────────────────────────
    ModelData                             m_sekiroModel;       ///< Raw model data for the player character
    std::shared_ptr<Material>             m_sekiroMaterial;    ///< Base material for the player
    std::vector<std::unique_ptr<MaterialInstance>> m_sekiroMatInstances; ///< Instances for character materials
    std::vector<size_t>                   m_sekiroSubMeshIndices; ///< Scene handles for all submeshes of the player

    // ── Player state ──────────────────────────────────────────────────────────
    glm::vec3 m_playerPosition = {8.74f, 1.20f, -8.90f};      ///< Current world position of the player
    size_t    m_playerCubeIdx  = (size_t)-1;                  ///< Primary submesh index (used for visibility checks)

    // ── Wind System ───────────────────────────────────────────────────────────
    float m_windTimer = 0.0f;        ///< Progress through current interval or duration
    float m_windInterval = 5.0f;     ///< Seconds between wind gusts
    float m_windDuration = 1.5f;     ///< How long a wind gust lasts
    bool  m_windActive = false;      ///< Whether wind is currently blowing

    glm::vec3 m_windDir = {1.0f, 0.0f, 0.0f}; ///< Direction of the current wind gust
    float m_windStrength = 5.0f;               ///< Speed/force of the current wind gust
};