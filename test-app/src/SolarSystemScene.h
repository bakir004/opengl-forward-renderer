#pragma once

#include "scene/Scene.h"
#include "core/MeshBuffer.h"
#include "core/ShaderProgram.h"
#include <memory>
#include <vector>
#include <glm/glm.hpp>

/// Solar system scene — v3:
///   • Moons each have a distinct solid color (own mesh per moon)
///   • Saturn rings built from proper annular vertex geometry (no scaled quads)
///   • Orbit rings (world-plane quads) removed
///   • Asteroids remain gray pyramids
///   • Starfield: ~300 tiny pyramids scattered at radius ~200 (static)
///
/// Camera starts in FreeFly above the ecliptic.
class SolarSystemScene : public Scene {
public:
    bool Setup();
    void OnUpdate(float deltaTime, KeyboardInput& input, MouseInput& mouse) override;

private:
    // ── Shared geometry ───────────────────────────────────────────────────────
    std::unique_ptr<ShaderProgram> m_shader;

    std::unique_ptr<MeshBuffer> m_asteroidMesh;  // belt rocks (grey pyramid)
    std::unique_ptr<MeshBuffer> m_shipMesh;      // player ship (rainbow cube)
    std::unique_ptr<MeshBuffer> m_starMesh;      // starfield (tiny white pyramid)

    // Per-planet solid-colored sphere mesh (one per planet)
    std::vector<std::unique_ptr<MeshBuffer>> m_planetMeshes;

    // Per-moon solid-colored sphere mesh (one per moon)
    std::vector<std::unique_ptr<MeshBuffer>> m_moonMeshes;

    // Saturn rings: 3 concentric annular meshes built from vertices
    std::unique_ptr<MeshBuffer> m_satRingInner;
    std::unique_ptr<MeshBuffer> m_satRingMid;
    std::unique_ptr<MeshBuffer> m_satRingOuter;

    // ── Per-body runtime data ─────────────────────────────────────────────────
    struct Planet {
        size_t    idx;
        float     orbitRadius;
        float     orbitSpeed;    // rad/s
        float     angle;         // current angle (rad)
        float     tiltDeg;
        float     selfRotSpeed;  // rad/s
        float     selfRotAngle;
        float     scale;
        glm::vec3 color;
    };

    struct Moon {
        size_t    idx;
        int       parentPlanet;
        float     orbitRadius;
        float     orbitSpeed;
        float     angle;
        float     scale;
        glm::vec3 color;
    };

    // Saturn's 3 ring layers: idx + scale multiplier (kept for struct compat)
    struct SaturnRingLayer {
        size_t idx;
        float  scaleMultiplier;
    };

    struct Asteroid {
        size_t idx;
        float  angle;
        float  orbitRadius;
        float  rotAngle;
        float  rotSpeed;
    };

    std::vector<Planet>          m_planets;
    std::vector<Moon>            m_moons;
    std::vector<SaturnRingLayer> m_saturnRings; // exactly 3 entries
    std::vector<Asteroid>        m_asteroids;

    size_t    m_sunIdx       = 0;
    size_t    m_playerIdx    = 0;
    glm::vec3 m_playerPos    = {0.0f, 12.0f, 80.0f};

    float     m_beltRotAngle = 0.0f;
    bool      m_isPaused     = false;

    // ── Helpers ───────────────────────────────────────────────────────────────
    void AddPlanet(float orbitRadius, float orbitSpeed, float startAngle,
                   float tiltDeg, float selfRotSpeed, float scale,
                   const glm::vec3& color);

    void AddMoon(int parentIdx, float orbitRadius, float orbitSpeed,
                 float startAngle, float scale, const glm::vec3& color);
};