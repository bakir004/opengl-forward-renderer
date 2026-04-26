#pragma once

#include "scene/Scene.h"
#include "core/MeshBuffer.h"
#include "core/ShaderProgram.h"
#include <memory>
#include <vector>
#include <glm/glm.hpp>

/// Solar system scene — v4 (refactored):
///   • Moons each have a distinct solid color (own mesh per moon)
///   • Saturn rings built from proper annular vertex geometry (no scaled quads)
///   • Orbit rings (world-plane quads) removed
///   • Asteroids remain gray pyramids
///   • Starfield: ~300 tiny pyramids scattered at radius ~200 (static)
///
/// Camera starts in FreeFly above the ecliptic.
class SolarSystemScene : public Scene
{
public:
    bool Setup();
    void OnUpdate(float deltaTime, IInputProvider &input) override;

private:
    // ── Shared geometry ───────────────────────────────────────────────────────
    std::unique_ptr<ShaderProgram> m_shader;

    std::unique_ptr<MeshBuffer> m_sunMesh;      // owned separately — not a planet
    std::unique_ptr<MeshBuffer> m_asteroidMesh; // belt rocks (grey pyramid)
    std::unique_ptr<MeshBuffer> m_shipMesh;     // player ship (rainbow cube)
    std::unique_ptr<MeshBuffer> m_starMesh;     // starfield (tiny white pyramid)

    // Per-planet solid-colored sphere mesh (one per planet, excludes sun)
    std::vector<std::unique_ptr<MeshBuffer>> m_planetMeshes;

    // Per-moon solid-colored sphere mesh (one per moon)
    std::vector<std::unique_ptr<MeshBuffer>> m_moonMeshes;

    // Saturn rings: 3 concentric annular meshes built from vertices
    std::unique_ptr<MeshBuffer> m_satRingInner;
    std::unique_ptr<MeshBuffer> m_satRingMid;
    std::unique_ptr<MeshBuffer> m_satRingOuter;

    // ── Per-body runtime data ─────────────────────────────────────────────────
    struct Planet
    {
        size_t idx = 0;
        float orbitRadius = 0.0f;
        float orbitSpeed = 0.0f; // rad/s
        float angle = 0.0f;      // current angle (rad)
        float tiltDeg = 0.0f;
        float selfRotSpeed = 0.0f; // rad/s
        float selfRotAngle = 0.0f;
        float scale = 1.0f;
        glm::vec3 color = {1.0f, 1.0f, 1.0f};
    };

    struct Moon
    {
        size_t idx = 0;
        int parentPlanet = 0;
        float orbitRadius = 0.0f;
        float orbitSpeed = 0.0f;
        float angle = 0.0f;
        float scale = 1.0f;
        glm::vec3 color = {1.0f, 1.0f, 1.0f};
    };

    // Saturn's 3 ring layers — only the scene-object index is needed at runtime.
    struct SaturnRingLayer
    {
        size_t idx = 0;
    };

    struct Asteroid
    {
        size_t idx = 0;
        float angle = 0.0f;
        float orbitRadius = 0.0f;
        float rotAngle = 0.0f;
        float rotSpeed = 0.0f;
    };

    std::vector<Planet> m_planets;
    std::vector<Moon> m_moons;
    std::vector<SaturnRingLayer> m_saturnRings; // exactly 3 entries
    std::vector<Asteroid> m_asteroids;

    size_t m_sunIdx = 0;
    size_t m_playerIdx = 0;
    glm::vec3 m_playerPos = {0.0f, 12.0f, 80.0f};

    float m_sunRotAngle = 0.0f; // moved out of static local in OnUpdate
    float m_beltRotAngle = 0.0f;
    bool m_isPaused = false;

    // Shooting Star variables
    std::unique_ptr<MeshBuffer> m_shootingStarMesh;
    size_t m_shootingStarIdx = (size_t)-1;
    bool m_starActive = false;
    float m_starTimer = 0.0f;
    glm::vec3 m_starStartPos;
    glm::vec3 m_starEndPos;
    float m_starDuration = 1.0f;

    // ── Helpers ───────────────────────────────────────────────────────────────
    void AddPlanet(float orbitRadius, float orbitSpeed, float startAngle,
                   float tiltDeg, float selfRotSpeed, float scale,
                   const glm::vec3 &color);

    void AddMoon(int parentIdx, float orbitRadius, float orbitSpeed,
                 float startAngle, float scale, const glm::vec3 &color);
};