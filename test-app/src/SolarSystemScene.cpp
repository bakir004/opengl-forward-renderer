#include "SolarSystemScene.h"
#include "core/Camera.h"
#include "core/KeyboardInput.h"
#include "core/MouseInput.h"
#include "core/Primitives.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>
#include <cmath>

// ── Data tables ───────────────────────────────────────────────────────────────

static const struct PlanetDef {
    float     orbitRadius;
    float     orbitSpeed;
    float     startAngle;
    float     tiltDeg;
    float     selfRotSpeed;
    float     scale;
    glm::vec3 color;
    const char* name;
} kPlanets[] = {
    {  14.0f, 0.80f, 0.0f,  0.0f, 2.0f,  0.55f, {0.62f, 0.60f, 0.58f}, "Mercury"},
    {  22.0f, 0.60f, 1.0f,  3.4f,-0.5f,  0.95f, {0.93f, 0.82f, 0.60f}, "Venus"  },
    {  31.0f, 0.50f, 2.1f, 23.5f, 1.8f,  1.00f, {0.25f, 0.55f, 0.90f}, "Earth"  },
    {  42.0f, 0.40f, 3.5f, 25.2f, 1.7f,  0.70f, {0.85f, 0.35f, 0.18f}, "Mars"   },
    {  62.0f, 0.23f, 0.8f,  3.1f, 3.5f,  2.50f, {0.88f, 0.72f, 0.52f}, "Jupiter"},
    {  84.0f, 0.15f, 1.6f, 26.7f, 1.5f,  2.10f, {0.94f, 0.87f, 0.62f}, "Saturn" },
    { 108.0f, 0.11f, 2.4f, 97.8f, 2.1f,  1.55f, {0.60f, 0.90f, 0.95f}, "Uranus" },
    { 132.0f, 0.09f, 4.2f, 28.3f, 2.3f,  1.50f, {0.20f, 0.35f, 0.90f}, "Neptune"},
};

// kSaturnPlanetIdx — position of Saturn inside kPlanets[]; used to sync rings.
static constexpr int kSaturnPlanetIdx = 5;

static const struct MoonDef {
    int       parent;
    float     orbitRadius;
    float     orbitSpeed;
    float     startAngle;
    float     scale;
    glm::vec3 color;
    const char* name;
} kMoons[] = {
    // Earth
    {2, 2.20f,  3.5f, 0.0f, 0.27f, {0.85f, 0.84f, 0.80f}, "Luna"    },
    // Mars
    {3, 1.60f,  5.5f, 0.0f, 0.14f, {0.50f, 0.33f, 0.22f}, "Phobos"  },
    {3, 2.60f,  3.2f, 1.5f, 0.10f, {0.68f, 0.58f, 0.45f}, "Deimos"  },
    // Jupiter — Galilean moons
    {4, 3.50f,  4.8f, 0.0f, 0.25f, {0.72f, 0.58f, 0.42f}, "Io"      },
    {4, 4.60f,  3.6f, 1.0f, 0.22f, {0.70f, 0.80f, 0.90f}, "Europa"  },
    {4, 6.00f,  2.9f, 2.2f, 0.38f, {0.55f, 0.50f, 0.48f}, "Ganymede"},
    {4, 7.50f,  2.1f, 3.8f, 0.35f, {0.42f, 0.32f, 0.28f}, "Callisto"},
    // Saturn
    {5, 6.00f,  3.0f, 0.5f, 0.25f, {0.90f, 0.88f, 0.82f}, "Titan"   },
    {5, 8.00f,  2.2f, 2.7f, 0.20f, {0.78f, 0.76f, 0.72f}, "Rhea"    },
    // Uranus
    {6, 3.20f,  2.8f, 1.2f, 0.18f, {0.62f, 0.70f, 0.75f}, "Titania" },
    // Neptune
    {7, 3.00f, -2.5f, 0.0f, 0.28f, {0.88f, 0.76f, 0.74f}, "Triton"  },
};

// ── Ring mesh factory (file-scope static, no longer a Setup() lambda) ─────────

static MeshBuffer MakeRingMesh(float innerRadius, float outerRadius,
                               const glm::vec3& color, int segments)
{
    // Vertex layout: [0..seg-1] = outer ring, [seg..2*seg-1] = inner ring.
    // The index buffer wraps with modulo so no seam vertex is duplicated.
    const int totalVerts = segments * 2;
    std::vector<float> verts;
    verts.reserve(static_cast<size_t>(totalVerts) * 6);

    const float step = glm::two_pi<float>() / static_cast<float>(segments);
    auto pushRingVerts = [&](float radius) {
        for (int i = 0; i < segments; ++i) {
            const float theta = step * static_cast<float>(i);
            verts.insert(verts.end(), {
                radius * std::cos(theta), 0.0f, radius * std::sin(theta),
                color.r, color.g, color.b
            });
        }
    };
    pushRingVerts(outerRadius);
    pushRingVerts(innerRadius);

    // Two triangles per quad segment, front + back faces.
    std::vector<uint32_t> indices;
    indices.reserve(static_cast<size_t>(segments) * 12);
    for (int i = 0; i < segments; ++i) {
        const auto o0 = static_cast<uint32_t>(i);
        const auto o1 = static_cast<uint32_t>((i + 1) % segments);
        const auto i0 = static_cast<uint32_t>(segments + i);
        const auto i1 = static_cast<uint32_t>(segments + (i + 1) % segments);
        // Front face
        indices.insert(indices.end(), {o0, o1, i0,  i0, o1, i1});
        // Back face (reversed winding — underside visible)
        indices.insert(indices.end(), {i0, o1, o0,  i1, o1, i0});
    }

    const VertexLayout layout({
        {0, 3, GL_FLOAT, GL_FALSE}, // position
        {1, 3, GL_FLOAT, GL_FALSE}, // color
    });
    return MeshBuffer(
        verts.data(),
        static_cast<GLsizeiptr>(verts.size() * sizeof(float)),
        static_cast<GLsizei>(totalVerts),
        indices.data(),
        static_cast<GLsizei>(indices.size()),
        layout,
        GL_STATIC_DRAW);
}

// ── Setup ─────────────────────────────────────────────────────────────────────

bool SolarSystemScene::Setup()
{
    spdlog::info("[SolarSystemScene] Setting up");

    m_shader = std::make_unique<ShaderProgram>(
        "assets/shaders/basic.vert",
        "assets/shaders/basic.frag");
    if (!m_shader->IsValid()) {
        spdlog::error("[SolarSystemScene] Shader failed — aborting");
        return false;
    }

    // ── Shared mesh assets ────────────────────────────────────────────────────

    m_asteroidMesh = std::make_unique<MeshBuffer>(
        GeneratePyramid({.colorMode = ColorMode::Solid,
                         .baseColor = {0.55f, 0.52f, 0.48f}})
            .CreateMeshBuffer());

    m_shipMesh = std::make_unique<MeshBuffer>(
        GenerateCube().CreateMeshBuffer());

    m_starMesh = std::make_unique<MeshBuffer>(
        GeneratePyramid({.colorMode = ColorMode::Solid,
                         .baseColor = {0.95f, 0.95f, 1.00f}})
            .CreateMeshBuffer());

    // ── Saturn ring meshes ────────────────────────────────────────────────────
    // Radii are expressed in world units relative to Saturn's body scale (2.10).
    constexpr float kSatBodyScale = 2.10f;
    m_satRingInner = std::make_unique<MeshBuffer>(
        MakeRingMesh(kSatBodyScale * 1.25f, kSatBodyScale * 1.95f, {0.82f, 0.74f, 0.55f}, 128));
    m_satRingMid   = std::make_unique<MeshBuffer>(
        MakeRingMesh(kSatBodyScale * 2.00f, kSatBodyScale * 2.75f, {0.72f, 0.65f, 0.48f}, 128));
    m_satRingOuter = std::make_unique<MeshBuffer>(
        MakeRingMesh(kSatBodyScale * 2.80f, kSatBodyScale * 3.60f, {0.55f, 0.50f, 0.38f}, 128));

    // ── Sun ───────────────────────────────────────────────────────────────────
    {
        m_sunMesh = std::make_unique<MeshBuffer>(
            GenerateSphere(1.0f, 32, {.colorMode  = ColorMode::Solid,
                                      .baseColor  = {1.0f, 0.90f, 0.25f},
                                      .doubleSided = true})
                .CreateMeshBuffer());

        RenderItem sun;
        sun.mesh   = m_sunMesh.get();
        sun.shader = m_shader.get();
        sun.transform.SetScale({5.5f, 5.5f, 5.5f});
        sun.transform.SetTranslation({0.0f, 0.0f, 0.0f});
        m_sunIdx = AddObject(sun);
    }

    // ── Planets ───────────────────────────────────────────────────────────────
    for (const auto& def : kPlanets) {
        AddPlanet(def.orbitRadius, def.orbitSpeed, def.startAngle,
                  def.tiltDeg, def.selfRotSpeed, def.scale, def.color);
        spdlog::info("[SolarSystemScene] Added planet: {}", def.name);
    }

    // ── Moons ─────────────────────────────────────────────────────────────────
    for (const auto& def : kMoons) {
        AddMoon(def.parent, def.orbitRadius, def.orbitSpeed,
                def.startAngle, def.scale, def.color);
        spdlog::info("[SolarSystemScene] Added moon: {}", def.name);
    }

    // ── Saturn ring scene objects ─────────────────────────────────────────────
    // Geometry is already in world-relative units; only position/tilt track Saturn.
    for (MeshBuffer* mesh : {m_satRingInner.get(), m_satRingMid.get(), m_satRingOuter.get()}) {
        RenderItem item;
        item.mesh   = mesh;
        item.shader = m_shader.get();
        m_saturnRings.push_back({AddObject(item)});
    }

    // ── Asteroid belt ─────────────────────────────────────────────────────────
    // Between Mars (idx 3) and Jupiter (idx 4).
    constexpr int   kAsteroidCount = 300;
    constexpr float kBeltInner     = 50.0f;
    constexpr float kBeltOuter     = 57.0f;
    constexpr float kBeltWidth     = kBeltOuter - kBeltInner; // = 7.0f
    for (int i = 0; i < kAsteroidCount; ++i) {
        const float fi = static_cast<float>(i);

        Asteroid a;
        a.angle       = (glm::two_pi<float>() / kAsteroidCount) * fi
                        + 0.05f * static_cast<float>(i % 7);
        // Use floating-point modulo so the radius fills the full belt width,
        // not just two discrete values as integer division would give.
        a.orbitRadius = kBeltInner + std::fmod(fi * 3.7f, kBeltWidth);
        a.rotAngle    = 0.0f;
        a.rotSpeed    = 0.5f + 0.3f * static_cast<float>(i % 5);

        const float s = 0.12f + 0.12f * static_cast<float>(i % 4);
        RenderItem item;
        item.mesh = m_asteroidMesh.get();
        item.shader = m_shader.get();
        item.transform.SetScale({s, s * 1.4f, s});
        item.transform.SetTranslation({
            std::cos(a.angle) * a.orbitRadius,
            0.0f,
            std::sin(a.angle) * a.orbitRadius
        });
        a.idx = AddObject(item);
        m_asteroids.push_back(a);
    }

    // ── Starfield ─────────────────────────────────────────────────────────────
    // 300 tiny near-white pyramids on a sphere of radius ~200.
    // Fibonacci sphere distribution gives even pole-to-pole coverage.
    constexpr int   kStarCount  = 300;
    constexpr float kStarRadius = 200.0f;
    const float goldenAngle = glm::pi<float>() * (3.0f - std::sqrt(5.0f));
    for (int i = 0; i < kStarCount; ++i) {
        const float fi    = static_cast<float>(i);
        const float y     = 1.0f - (fi / (kStarCount - 1)) * 2.0f;
        const float r     = std::sqrt(std::max(0.0f, 1.0f - y * y));
        const float theta = goldenAngle * fi;
        const float s     = 0.18f + 0.22f * static_cast<float>(i % 5) / 4.0f;

        RenderItem star;
        star.mesh   = m_starMesh.get();
        star.shader = m_shader.get();
        star.transform.SetScale({s, s, s});
        star.transform.SetTranslation({
            std::cos(theta) * r * kStarRadius,
            y * kStarRadius,
            std::sin(theta) * r * kStarRadius
        });
        // Vary orientation so pyramids don't all point the same way.
        star.transform.SetRotationEulerDegrees({
            static_cast<float>((i * 73)  % 360),
            static_cast<float>((i * 137) % 360),
            0.0f
        });
        AddObject(star);
    }

    // ── Player ship ───────────────────────────────────────────────────────────
    {
        RenderItem ship;
        ship.mesh   = m_shipMesh.get();
        ship.shader = m_shader.get();
        ship.transform.SetScale({0.08f, 0.05f, 0.18f});
        ship.transform.SetTranslation(m_playerPos);
        m_playerIdx = AddObject(ship);
    }

    // ── Camera ────────────────────────────────────────────────────────────────
    m_cameraFreeFlySpeed = 20.0f;
    m_cameraFirstPersonSpeed = 20.0f;
    m_cameraThirdPersonSpeed = 20.0f;
    m_cameraOrbitRadius = 1.5f;

    Camera cam;
    cam.SetPosition({0.0f, 200.0f, 0.0f});
    cam.SetOrientation(0.0f, -90.0f);
    SetCamera(cam);
    SetClearColor({0.01f, 0.01f, 0.05f, 1.0f}); // deep space black

    spdlog::info("[SolarSystemScene] Ready — {} planets, {} moons, {} asteroids, {} stars",
                 m_planets.size(), m_moons.size(), m_asteroids.size(), kStarCount);
    return true;
}

// ── Helper: AddPlanet ─────────────────────────────────────────────────────────

void SolarSystemScene::AddPlanet(float orbitRadius, float orbitSpeed,
                                 float startAngle, float tiltDeg,
                                 float selfRotSpeed, float scale,
                                 const glm::vec3& color)
{
    // Build mesh first so we can take its raw pointer before moving into the vector.
    m_planetMeshes.push_back(std::make_unique<MeshBuffer>(
        GenerateSphere(1.0f, 32, {.colorMode   = ColorMode::Solid,
                                  .baseColor   = color,
                                  .doubleSided = true})
            .CreateMeshBuffer()));

    const float x = std::cos(startAngle) * orbitRadius;
    const float z = std::sin(startAngle) * orbitRadius;

    RenderItem item;
    item.mesh   = m_planetMeshes.back().get();
    item.shader = m_shader.get();
    item.transform.SetScale({scale, scale, scale});
    item.transform.SetTranslation({x, 0.0f, z});
    item.transform.SetRotationEulerDegrees({tiltDeg, 0.0f, 0.0f});

    m_planets.push_back(Planet{
        .idx          = AddObject(item),
        .orbitRadius  = orbitRadius,
        .orbitSpeed   = orbitSpeed,
        .angle        = startAngle,
        .tiltDeg      = tiltDeg,
        .selfRotSpeed = selfRotSpeed,
        .selfRotAngle = 0.0f,
        .scale        = scale,
        .color        = color,
    });
}

// ── Helper: AddMoon ───────────────────────────────────────────────────────────

void SolarSystemScene::AddMoon(int parentIdx, float orbitRadius,
                               float orbitSpeed, float startAngle,
                               float scale, const glm::vec3& color)
{
    const Planet& parent = m_planets[parentIdx];
    const float px = std::cos(parent.angle) * parent.orbitRadius;
    const float pz = std::sin(parent.angle) * parent.orbitRadius;

    m_moonMeshes.push_back(std::make_unique<MeshBuffer>(
        GenerateSphere(1.0f, 24, {.colorMode   = ColorMode::Solid,
                                  .baseColor   = color,
                                  .doubleSided = true})
            .CreateMeshBuffer()));

    RenderItem item;
    item.mesh   = m_moonMeshes.back().get();
    item.shader = m_shader.get();
    item.transform.SetScale({scale, scale, scale});
    item.transform.SetTranslation({
        px + std::cos(startAngle) * orbitRadius,
        0.0f,
        pz + std::sin(startAngle) * orbitRadius
    });

    m_moons.push_back(Moon{
        .idx          = AddObject(item),
        .parentPlanet = parentIdx,
        .orbitRadius  = orbitRadius,
        .orbitSpeed   = orbitSpeed,
        .angle        = startAngle,
        .scale        = scale,
        .color        = color,
    });
}


// ── OnUpdate ──────────────────────────────────────────────────────────────────

void SolarSystemScene::OnUpdate(float deltaTime, KeyboardInput& input, MouseInput& mouse)
{
    // M — toggle pause
    if (input.IsKeyPressed(GLFW_KEY_M)) {
        m_isPaused = !m_isPaused;
        spdlog::info("[SolarSystemScene] {}", m_isPaused ? "PAUSED" : "UNPAUSED");
    }

    glm::vec3 moveDirXZ;
    UpdateStandardCameraAndPlayer(deltaTime, input, mouse, m_playerPos, moveDirXZ, 0.0f);

    // Sync ship transform
    {
        auto& t = GetObject(m_playerIdx).transform;
        t.SetTranslation(m_playerPos);
        if (glm::length(moveDirXZ) > 0.001f) {
            const glm::vec3 d = glm::normalize(moveDirXZ);
            t.SetRotationEulerDegrees({0.0f, glm::degrees(std::atan2(d.x, d.z)), 0.0f});
        }
    }

    // ── Celestial body animations (pause-aware) ───────────────────────────────
    if (!m_isPaused) {
        // Sun self-rotation (m_sunRotAngle is a proper member, not a static local).
        m_sunRotAngle += 0.10f * deltaTime;
        GetObject(m_sunIdx).transform.SetRotationEulerDegrees(
            {0.0f, glm::degrees(m_sunRotAngle), 0.0f});

        // Planets
        for (auto& p : m_planets) {
            p.angle        += p.orbitSpeed   * deltaTime;
            p.selfRotAngle += p.selfRotSpeed * deltaTime;

            auto& t = GetObject(p.idx).transform;
            t.SetTranslation({std::cos(p.angle) * p.orbitRadius,
                              0.0f,
                              std::sin(p.angle) * p.orbitRadius});
            t.SetRotationEulerDegrees({p.tiltDeg, glm::degrees(p.selfRotAngle), 0.0f});
        }

        // Moons
        for (auto& m : m_moons) {
            m.angle += m.orbitSpeed * deltaTime;

            const Planet& parent = m_planets[m.parentPlanet];
            GetObject(m.idx).transform.SetTranslation({
                std::cos(parent.angle) * parent.orbitRadius + std::cos(m.angle) * m.orbitRadius,
                0.0f,
                std::sin(parent.angle) * parent.orbitRadius + std::sin(m.angle) * m.orbitRadius
            });
        }

        // Saturn ring layers — follow Saturn's position and axial tilt.
        {
            const Planet& saturn = m_planets[kSaturnPlanetIdx];
            const glm::vec3 saturnPos = {
                std::cos(saturn.angle) * saturn.orbitRadius,
                0.0f,
                std::sin(saturn.angle) * saturn.orbitRadius
            };
            for (const auto& sr : m_saturnRings) {
                auto& t = GetObject(sr.idx).transform;
                t.SetTranslation(saturnPos);
                t.SetRotationEulerDegrees(
                    {saturn.tiltDeg, glm::degrees(saturn.selfRotAngle), 0.0f});
                t.SetScale({1.0f, 1.0f, 1.0f});
            }
        }

        // Asteroid belt
        m_beltRotAngle += 0.04f * deltaTime;
        for (auto& a : m_asteroids) {
            a.rotAngle += a.rotSpeed * deltaTime;
            const float angle = a.angle + m_beltRotAngle;
            auto& t = GetObject(a.idx).transform;
            t.SetTranslation({std::cos(angle) * a.orbitRadius,
                              0.0f,
                              std::sin(angle) * a.orbitRadius});
            t.SetRotationEulerDegrees({0.0f, glm::degrees(a.rotAngle), 0.0f});
        }
    }

}