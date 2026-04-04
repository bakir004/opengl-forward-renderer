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

// ── Planet data table ─────────────────────────────────────────────────────────
// Scales bumped ~3-4× vs v1. Orbit radii spaced proportionally so planets
// don't overlap at their larger sizes. Ship (0.08 × 0.05 × 0.18) is tiny by
// comparison — feels like a real vessel navigating between worlds.
static const struct PlanetDef {
    float orbitRadius;
    float orbitSpeed; // rad/s  (kept same — animation speed unchanged)
    float startAngle; // rad
    float tiltDeg;
    float selfRotSpeed; // rad/s
    float scale; // world-unit radius
    glm::vec3 color;
    const char *name;
} kPlanets[] = {
    // Mercury — small fast grey rock
    {14.0f, 0.80f, 0.0f, 0.0f, 2.0f, 0.55f, {0.62f, 0.60f, 0.58f}, "Mercury"},
    // Venus — creamy yellow, slow retrograde spin
    {22.0f, 0.60f, 1.0f, 3.4f, -0.5f, 0.95f, {0.93f, 0.82f, 0.60f}, "Venus"},
    // Earth — blue with a hint of green
    {31.0f, 0.50f, 2.1f, 23.5f, 1.8f, 1.00f, {0.25f, 0.55f, 0.90f}, "Earth"},
    // Mars — rusty red
    {42.0f, 0.40f, 3.5f, 25.2f, 1.7f, 0.70f, {0.85f, 0.35f, 0.18f}, "Mars"},
    // Jupiter — biggest planet, warm banded orange
    {62.0f, 0.23f, 0.8f, 3.1f, 3.5f, 2.50f, {0.88f, 0.72f, 0.52f}, "Jupiter"},
    // Saturn — pale gold; rings added separately below
    {84.0f, 0.15f, 1.6f, 26.7f, 1.5f, 2.10f, {0.94f, 0.87f, 0.62f}, "Saturn"},
    // Uranus — icy blue, extreme axial tilt
    {108.0f, 0.11f, 2.4f, 97.8f, 2.1f, 1.55f, {0.60f, 0.90f, 0.95f}, "Uranus"},
    // Neptune — deep cobalt blue
    {132.0f, 0.09f, 4.2f, 28.3f, 2.3f, 1.50f, {0.20f, 0.35f, 0.90f}, "Neptune"},
};

// ── Moon data ─────────────────────────────────────────────────────────────────
// orbitRadius / scale kept proportional to updated planet sizes.
static const struct MoonDef {
    int parent;
    float orbitRadius;
    float orbitSpeed;
    float startAngle;
    float scale;
    glm::vec3 color;
    const char *name;
} kMoons[] = {
    // Earth — Luna (pale grey-white)
    {2, 2.20f, 3.5f, 0.0f, 0.27f, {0.85f, 0.84f, 0.80f}, "Luna"},
    // Mars — Phobos (dark reddish-brown) / Deimos (lighter tan)
    {3, 1.60f, 5.5f, 0.0f, 0.14f, {0.50f, 0.33f, 0.22f}, "Phobos"},
    {3, 2.60f, 3.2f, 1.5f, 0.10f, {0.68f, 0.58f, 0.45f}, "Deimos"},
    // Jupiter — 4 Galilean moons
    {4, 3.50f, 4.8f, 0.0f, 0.25f, {0.72f, 0.58f, 0.42f}, "Io"}, // sulphur orange
    {4, 4.60f, 3.6f, 1.0f, 0.22f, {0.70f, 0.80f, 0.90f}, "Europa"}, // icy blue-white
    {4, 6.00f, 2.9f, 2.2f, 0.38f, {0.55f, 0.50f, 0.48f}, "Ganymede"}, // dark grey
    {4, 7.50f, 2.1f, 3.8f, 0.35f, {0.42f, 0.32f, 0.28f}, "Callisto"}, // cratered dark
    // Saturn — 2 moons
    {5, 6.00f, 3.0f, 0.5f, 0.25f, {0.90f, 0.88f, 0.82f}, "Titan"}, // hazy cream
    {5, 8.00f, 2.2f, 2.7f, 0.20f, {0.78f, 0.76f, 0.72f}, "Rhea"}, // pale grey
    // Uranus — 1 moon
    {6, 3.20f, 2.8f, 1.2f, 0.18f, {0.62f, 0.70f, 0.75f}, "Titania"}, // cool blue-grey
    // Neptune — Triton (retrograde, pinkish nitrogen ice)
    {7, 3.00f, -2.5f, 0.0f, 0.28f, {0.88f, 0.76f, 0.74f}, "Triton"},
};

// ─────────────────────────────────────────────────────────────────────────────

bool SolarSystemScene::Setup() {
    spdlog::info("[SolarSystemScene] Setting up (v3)");

    m_shader = std::make_unique<ShaderProgram>(
        "assets/shaders/basic.vert",
        "assets/shaders/basic.frag");
    if (!m_shader->IsValid()) {
        spdlog::error("[SolarSystemScene] Shader failed — aborting");
        return false;
    }

    // ── Shared geometry ───────────────────────────────────────────────────────

    // Asteroid belt rocks
    m_asteroidMesh = std::make_unique<MeshBuffer>(
        GeneratePyramid({
            .colorMode = ColorMode::Solid,
            .baseColor = {0.55f, 0.52f, 0.48f}
        }).CreateMeshBuffer());

    // Player ship — rainbow cube; tiny scale makes it feel like a spacecraft
    m_shipMesh = std::make_unique<MeshBuffer>(
        GenerateCube().CreateMeshBuffer());

    // Starfield — near-white pyramid, very tiny (scaled in Setup loop below)
    m_starMesh = std::make_unique<MeshBuffer>(
        GeneratePyramid({
            .colorMode = ColorMode::Solid,
            .baseColor = {0.95f, 0.95f, 1.00f}
        }).CreateMeshBuffer());

    // Saturn rings — built as proper annular meshes from vertices.
    // Each ring is a flat triangle-strip annulus in the XZ plane,
    // with innerRadius and outerRadius defining the band width.
    // We generate 3 concentric bands: inner (warm gold), mid (tan), outer (dusty).
    // Builds a flat annular ring mesh in the XZ plane as an indexed triangle list.
    // Vertex layout matches the engine's standard interleaved format:
    //   attrib 0: position  (vec3, offset  0)
    //   attrib 1: color     (vec3, offset 12)
    //   stride = 6 floats = 24 bytes
    auto MakeRingMesh = [](float innerRadius, float outerRadius,
                           const glm::vec3 &color, int segments) -> MeshBuffer {
        // 'segments' outer verts + 'segments' inner verts, no duplicated seam vertex
        // because the index buffer wraps around with modulo.
        const int totalVerts = segments * 2; // [0..seg-1] = outer, [seg..2*seg-1] = inner
        std::vector<float> verts;
        verts.reserve(static_cast<size_t>(totalVerts) * 6);

        const float step = glm::two_pi<float>() / static_cast<float>(segments);
        for (int i = 0; i < segments; ++i) {
            const float theta = step * static_cast<float>(i);
            const float c = std::cos(theta), s = std::sin(theta);
            // Outer ring vertex
            verts.insert(verts.end(), {
                             outerRadius * c, 0.0f, outerRadius * s,
                             color.r, color.g, color.b
                         });
        }
        for (int i = 0; i < segments; ++i) {
            const float theta = step * static_cast<float>(i);
            const float c = std::cos(theta), s = std::sin(theta);
            // Inner ring vertex
            verts.insert(verts.end(), {
                             innerRadius * c, 0.0f, innerRadius * s,
                             color.r, color.g, color.b
                         });
        }

        // Two triangles per segment quad:
        //   outer[i], outer[i+1], inner[i]
        //   inner[i], outer[i+1], inner[i+1]
        // Indices wrap via modulo so no seam vertex is needed.
        std::vector<uint32_t> indices;
        indices.reserve(static_cast<size_t>(segments) * 12);
        for (int i = 0; i < segments; ++i) {
            const uint32_t o0 = static_cast<uint32_t>(i);
            const uint32_t o1 = static_cast<uint32_t>((i + 1) % segments);
            const uint32_t i0 = static_cast<uint32_t>(segments + i);
            const uint32_t i1 = static_cast<uint32_t>(segments + (i + 1) % segments);
            // Front face
            indices.insert(indices.end(), {
                               o0, o1, i0,
                               i0, o1, i1
                           });
            // Back face (reversed winding so the underside is also visible)
            indices.insert(indices.end(), {
                               i0, o1, o0,
                               i1, o1, i0
                           });
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
    };

    // Saturn's body scale is 2.10; rings start just outside the surface.
    constexpr float kSatBodyScale = 2.10f;
    m_satRingInner = std::make_unique<MeshBuffer>(
        MakeRingMesh(kSatBodyScale * 1.25f, kSatBodyScale * 1.95f,
                     {0.82f, 0.74f, 0.55f}, 128)); // warm gold band
    m_satRingMid = std::make_unique<MeshBuffer>(
        MakeRingMesh(kSatBodyScale * 2.00f, kSatBodyScale * 2.75f,
                     {0.72f, 0.65f, 0.48f}, 128)); // mid tan band
    m_satRingOuter = std::make_unique<MeshBuffer>(
        MakeRingMesh(kSatBodyScale * 2.80f, kSatBodyScale * 3.60f,
                     {0.55f, 0.50f, 0.38f}, 128)); // dusty outer band

    // ── Sun ───────────────────────────────────────────────────────────────────
    {
        std::unique_ptr<MeshBuffer> sunMesh = std::make_unique<MeshBuffer>(
            GenerateSphere(1.0f, 32, {
                               .colorMode = ColorMode::Solid,
                               .baseColor = {1.0f, 0.90f, 0.25f}, // warm yellow
                               .doubleSided = true
                           }).CreateMeshBuffer());

        RenderItem sun;
        sun.mesh = sunMesh.get();
        sun.shader = m_shader.get();
        sun.transform.SetScale({5.5f, 5.5f, 5.5f});
        sun.transform.SetTranslation({0.0f, 0.0f, 0.0f});
        m_sunIdx = AddObject(sun);

        m_planetMeshes.push_back(std::move(sunMesh)); // reuse m_planetMeshes to own it
    }

    // ── Planets ───────────────────────────────────────────────────────────────
    for (const auto &def: kPlanets) {
        AddPlanet(def.orbitRadius, def.orbitSpeed, def.startAngle,
                  def.tiltDeg, def.selfRotSpeed, def.scale, def.color);
        spdlog::info("[SolarSystemScene] Added planet: {}", def.name);
    }

    // ── Moons ─────────────────────────────────────────────────────────────────
    for (const auto &def: kMoons)
        AddMoon(def.parent, def.orbitRadius, def.orbitSpeed, def.startAngle, def.scale, def.color);

    // ── Saturn ring detail (planet index 5) ───────────────────────────────────
    // Three concentric annular meshes (proper vertex rings, not scaled quads).
    // Their geometry is already in world-relative units (Saturn body = 2.10).
    // We only need to track idx + follow Saturn's position/tilt each frame.
    {
        for (MeshBuffer *mesh: {m_satRingInner.get(), m_satRingMid.get(), m_satRingOuter.get()}) {
            RenderItem item;
            item.mesh = mesh;
            item.shader = m_shader.get();
            SaturnRingLayer sr;
            sr.scaleMultiplier = 1.0f; // geometry already sized; kept for struct compat
            sr.idx = AddObject(item);
            m_saturnRings.push_back(sr);
        }
    }

    // ── Asteroid belt (between Mars[3] and Jupiter[4]) ────────────────────────
    // Belt inner/outer radii scaled to match new orbit layout
    constexpr int kAsteroidCount = 80;
    constexpr float kBeltInner = 50.0f;
    constexpr float kBeltOuter = 57.0f;
    for (int i = 0; i < kAsteroidCount; ++i) {
        Asteroid a;
        a.angle = (glm::two_pi<float>() / kAsteroidCount) * i
                  + 0.05f * static_cast<float>(i % 7);
        a.orbitRadius = kBeltInner + (kBeltOuter - kBeltInner)
                        * (static_cast<float>(i % 11) / 10.0f);
        a.rotAngle = 0.0f;
        a.rotSpeed = 0.5f + 0.3f * static_cast<float>(i % 5);

        RenderItem item;
        item.mesh = m_asteroidMesh.get();
        item.shader = m_shader.get();
        float s = 0.12f + 0.12f * static_cast<float>(i % 4);
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
    // 300 tiny near-white pyramids scattered on a sphere of radius ~200.
    // Deterministic placement using a Fibonacci sphere distribution so stars
    // spread evenly without clustering at the poles.
    constexpr int kStarCount = 300;
    constexpr float kStarRadius = 200.0f;
    const float goldenAngle = glm::pi<float>() * (3.0f - std::sqrt(5.0f));
    for (int i = 0; i < kStarCount; ++i) {
        const float y = 1.0f - (static_cast<float>(i) / (kStarCount - 1)) * 2.0f;
        const float r = std::sqrt(std::max(0.0f, 1.0f - y * y));
        const float theta = goldenAngle * static_cast<float>(i);
        const float x = std::cos(theta) * r;
        const float z = std::sin(theta) * r;

        // Vary star size slightly for depth perception
        const float s = 0.18f + 0.22f * static_cast<float>(i % 5) / 4.0f;

        RenderItem star;
        star.mesh = m_starMesh.get();
        star.shader = m_shader.get();
        star.transform.SetScale({s, s, s});
        star.transform.SetTranslation({x * kStarRadius, y * kStarRadius, z * kStarRadius});
        // Random-ish rotation so pyramids don't all point the same way
        star.transform.SetRotationEulerDegrees({
            static_cast<float>((i * 73) % 360),
            static_cast<float>((i * 137) % 360),
            0.0f
        });
        AddObject(star); // indices not needed — stars are fully static
    }

    // ── Player ship ───────────────────────────────────────────────────────────
    {
        RenderItem ship;
        ship.mesh = m_shipMesh.get();
        ship.shader = m_shader.get();
        // Very small — dwarfed by even Mercury
        ship.transform.SetScale({0.08f, 0.05f, 0.18f});
        ship.transform.SetTranslation(m_playerPos);
        m_playerIdx = AddObject(ship);
    }

    // ── Camera ────────────────────────────────────────────────────────────────
    // Positioned high above the ecliptic, angled down to see the full system.
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
                                 const glm::vec3 &color) {
    Planet p;
    p.orbitRadius = orbitRadius;
    p.orbitSpeed = orbitSpeed;
    p.angle = startAngle;
    p.tiltDeg = tiltDeg;
    p.selfRotSpeed = selfRotSpeed;
    p.selfRotAngle = 0.0f;
    p.scale = scale;
    p.color = color;

    // Each planet gets its own solid-colored sphere mesh (baked vertex colors),
    // matching how SampleScene used ColorMode::Solid for m_solidCube.
    std::unique_ptr<MeshBuffer> mesh = std::make_unique<MeshBuffer>(
        GenerateSphere(1.0f, 32, {
                           .colorMode = ColorMode::Solid,
                           .baseColor = color,
                           .doubleSided = true
                       }).CreateMeshBuffer());

    const float x = std::cos(startAngle) * orbitRadius;
    const float z = std::sin(startAngle) * orbitRadius;

    RenderItem item;
    item.shader = m_shader.get();
    item.transform.SetScale({scale, scale, scale});
    item.transform.SetTranslation({x, 0.0f, z});
    item.transform.SetRotationEulerDegrees({tiltDeg, 0.0f, 0.0f});

    m_planetMeshes.push_back(std::move(mesh));
    item.mesh = m_planetMeshes.back().get();

    p.idx = AddObject(item);
    m_planets.push_back(p);
}

// ── Helper: AddMoon ───────────────────────────────────────────────────────────

void SolarSystemScene::AddMoon(int parentIdx, float orbitRadius,
                               float orbitSpeed, float startAngle,
                               float scale, const glm::vec3 &color) {
    const Planet &parent = m_planets[parentIdx];

    const float px = std::cos(parent.angle) * parent.orbitRadius;
    const float pz = std::sin(parent.angle) * parent.orbitRadius;
    const float mx = px + std::cos(startAngle) * orbitRadius;
    const float mz = pz + std::sin(startAngle) * orbitRadius;

    // Each moon gets its own solid-colored sphere mesh
    std::unique_ptr<MeshBuffer> mesh = std::make_unique<MeshBuffer>(
        GenerateSphere(1.0f, 24, {
                           .colorMode = ColorMode::Solid,
                           .baseColor = color,
                           .doubleSided = true
                       }).CreateMeshBuffer());

    RenderItem item;
    item.shader = m_shader.get();
    item.transform.SetScale({scale, scale, scale});
    item.transform.SetTranslation({mx, 0.0f, mz});

    m_moonMeshes.push_back(std::move(mesh));
    item.mesh = m_moonMeshes.back().get();

    Moon m;
    m.parentPlanet = parentIdx;
    m.orbitRadius = orbitRadius;
    m.orbitSpeed = orbitSpeed;
    m.angle = startAngle;
    m.scale = scale;
    m.idx = AddObject(item);
    m_moons.push_back(m);
}

// ── OnUpdate ──────────────────────────────────────────────────────────────────

void SolarSystemScene::OnUpdate(float deltaTime, KeyboardInput &input, MouseInput &mouse) {
    // TAB — toggle mouse capture
    if (input.IsKeyPressed(GLFW_KEY_TAB))
        mouse.SetCaptured(!mouse.IsCaptured());

    // M — toggle pause
    if (input.IsKeyPressed(GLFW_KEY_M)) {
        m_isPaused = !m_isPaused;
        spdlog::info("[SolarSystemScene] {}", m_isPaused ? "PAUSED" : "UNPAUSED");
    }

    Camera &cam = GetCamera();

    // Camera mode switching
    if (input.IsKeyPressed(GLFW_KEY_F1)) {
        cam.SetMode(CameraMode::FreeFly);
        spdlog::info("[Camera] FreeFly");
    }
    if (input.IsKeyPressed(GLFW_KEY_F2)) {
        cam.SetMode(CameraMode::FirstPerson);
        spdlog::info("[Camera] FirstPerson");
    }
    if (input.IsKeyPressed(GLFW_KEY_F3)) {
        cam.SetMode(CameraMode::ThirdPerson);
        cam.SetOrbitTarget(m_playerPos);
        cam.SetOrbitRadius(1.5f); // tight orbit around the tiny ship
        spdlog::info("[Camera] ThirdPerson — orbiting ship");
    }

    // Mouse look
    if (mouse.IsCaptured())
        cam.Rotate(mouse.GetDeltaX() * 0.1f, -mouse.GetDeltaY() * 0.1f);

    // ── Movement input ────────────────────────────────────────────────────────
    // Fly speed bumped to match the larger scene scale
    constexpr float kFlySpeed = 20.0f;
    constexpr float kShipSpeed = 20.0f;
    float fwd = 0.0f, right = 0.0f, up = 0.0f;
    if (input.IsKeyDown(GLFW_KEY_W)) fwd += 1.0f;
    if (input.IsKeyDown(GLFW_KEY_S)) fwd -= 1.0f;
    if (input.IsKeyDown(GLFW_KEY_D)) right += 1.0f;
    if (input.IsKeyDown(GLFW_KEY_A)) right -= 1.0f;
    if (input.IsKeyDown(GLFW_KEY_SPACE)) up += 1.0f;
    if (input.IsKeyDown(GLFW_KEY_LEFT_CONTROL)) up -= 1.0f;

    glm::vec3 moveDirXZ(0.0f);

    if (cam.GetMode() == CameraMode::FreeFly) {
        if (fwd > 0.0f) cam.Move(CameraDirection::Forward, kFlySpeed, deltaTime);
        if (fwd < 0.0f) cam.Move(CameraDirection::Backward, kFlySpeed, deltaTime);
        if (right > 0.0f) cam.Move(CameraDirection::Right, kFlySpeed, deltaTime);
        if (right < 0.0f) cam.Move(CameraDirection::Left, kFlySpeed, deltaTime);
        if (up > 0.0f) cam.Move(CameraDirection::Up, kFlySpeed, deltaTime);
        if (up < 0.0f) cam.Move(CameraDirection::Down, kFlySpeed, deltaTime);
    } else if (cam.GetMode() == CameraMode::FirstPerson) {
        const glm::vec3 fwdXZ = glm::normalize(glm::vec3(cam.GetForward().x, 0.0f, cam.GetForward().z));
        const glm::vec3 rightXZ = glm::normalize(glm::vec3(cam.GetRight().x, 0.0f, cam.GetRight().z));
        moveDirXZ = fwdXZ * fwd + rightXZ * right;
        m_playerPos += moveDirXZ * (kShipSpeed * deltaTime);
        m_playerPos.y += up * kShipSpeed * deltaTime;
        cam.SetPosition(m_playerPos);
    } else if (cam.GetMode() == CameraMode::ThirdPerson) {
        const glm::vec3 fwdXZ = glm::normalize(glm::vec3(cam.GetForward().x, 0.0f, cam.GetForward().z));
        const glm::vec3 rightXZ = glm::normalize(glm::vec3(cam.GetRight().x, 0.0f, cam.GetRight().z));
        moveDirXZ = fwdXZ * fwd + rightXZ * right;
        m_playerPos += moveDirXZ * (kShipSpeed * deltaTime);
        m_playerPos.y += up * kShipSpeed * deltaTime;
        cam.SetOrbitTarget(m_playerPos);
    }

    // Sync ship transform
    {
        auto &t = GetObject(m_playerIdx).transform;
        t.SetTranslation(m_playerPos);
        if (glm::length(moveDirXZ) > 0.001f) {
            const glm::vec3 d = glm::normalize(moveDirXZ);
            t.SetRotationEulerDegrees({0.0f, glm::degrees(std::atan2(d.x, d.z)), 0.0f});
        }
    }

    // ── Celestial body animations (pause-aware) ───────────────────────────────
    if (!m_isPaused) {
        // ── Sun: slow self-rotation ───────────────────────────────────────────
        {
            static float sunRot = 0.0f;
            sunRot += 0.10f * deltaTime;
            GetObject(m_sunIdx).transform.SetRotationEulerDegrees(
                {0.0f, glm::degrees(sunRot), 0.0f});
        }

        // ── Planets ───────────────────────────────────────────────────────────
        for (auto &p: m_planets) {
            p.angle += p.orbitSpeed * deltaTime;
            p.selfRotAngle += p.selfRotSpeed * deltaTime;

            const float x = std::cos(p.angle) * p.orbitRadius;
            const float z = std::sin(p.angle) * p.orbitRadius;

            auto &t = GetObject(p.idx).transform;
            t.SetTranslation({x, 0.0f, z});
            t.SetRotationEulerDegrees({p.tiltDeg, glm::degrees(p.selfRotAngle), 0.0f});
        }

        // ── Moons ─────────────────────────────────────────────────────────────
        for (auto &m: m_moons) {
            m.angle += m.orbitSpeed * deltaTime;

            const Planet &parent = m_planets[m.parentPlanet];
            const float px = std::cos(parent.angle) * parent.orbitRadius;
            const float pz = std::sin(parent.angle) * parent.orbitRadius;

            GetObject(m.idx).transform.SetTranslation({
                px + std::cos(m.angle) * m.orbitRadius,
                0.0f,
                pz + std::sin(m.angle) * m.orbitRadius
            });
        }

        // ── Saturn ring layers ────────────────────────────────────────────────
        // All three annular rings follow Saturn's position and axial tilt.
        // No scale override needed — vertices are already in world units.
        {
            const Planet &saturn = m_planets[5];
            const float px = std::cos(saturn.angle) * saturn.orbitRadius;
            const float pz = std::sin(saturn.angle) * saturn.orbitRadius;

            for (const auto &sr: m_saturnRings) {
                auto &t = GetObject(sr.idx).transform;
                t.SetTranslation({px, 0.0f, pz});
                // Tilt to match Saturn's axial tilt; rings lie in its equatorial plane
                t.SetRotationEulerDegrees({saturn.tiltDeg, glm::degrees(saturn.selfRotAngle), 0.0f});
                t.SetScale({1.0f, 1.0f, 1.0f});
            }
        }

        // ── Asteroid belt ─────────────────────────────────────────────────────
        m_beltRotAngle += 0.04f * deltaTime;
        for (auto &a: m_asteroids) {
            a.rotAngle += a.rotSpeed * deltaTime;
            const float effectiveAngle = a.angle + m_beltRotAngle;
            auto &t = GetObject(a.idx).transform;
            t.SetTranslation({
                std::cos(effectiveAngle) * a.orbitRadius,
                0.0f,
                std::sin(effectiveAngle) * a.orbitRadius
            });
            t.SetRotationEulerDegrees({0.0f, glm::degrees(a.rotAngle), 0.0f});
        }
    }

    // ── Debug log ─────────────────────────────────────────────────────────────
    const bool moving = fwd != 0.0f || right != 0.0f || up != 0.0f;
    const bool rotating = mouse.IsCaptured() &&
                          (mouse.GetDeltaX() != 0.0f || mouse.GetDeltaY() != 0.0f);
    if (moving || rotating) {
        const glm::vec3 pos = cam.GetPosition();
        spdlog::info("[Camera] pos=({:.2f},{:.2f},{:.2f})  yaw={:.1f}°  pitch={:.1f}°",
                     pos.x, pos.y, pos.z, cam.GetYaw(), cam.GetPitch());
    }
}
