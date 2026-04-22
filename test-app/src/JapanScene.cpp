#include "JapanScene.h"
#include "core/Camera.h"
#include "core/KeyboardInput.h"
#include "core/MouseInput.h"
#include "core/Primitives.h"
#include "core/Texture2D.h"
#include "scene/LightBuilder.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <spdlog/spdlog.h>
#include "core/ShaderProgram.h"

// ─────────────────────────────────────────────────────────────────────────────
// Small utility: random float in range [min, max]
// Keeps code cleaner and avoids repeated rand() boilerplate
// ─────────────────────────────────────────────────────────────────────────────
static float Rand(float min, float max)
{
    return min + (max - min) * ((float)(rand() % 100) / 100.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Setup
// ─────────────────────────────────────────────────────────────────────────────
bool JapanScene::Setup()
{
    spdlog::info("[JapanScene] Setting up");
    SetSceneName("Japan");

    // ── Shader ───────────────────────────────────────────────────────────────
    auto meshShader = AssetImporter::LoadShader(
        "assets/shaders/mesh.vert",
        "assets/shaders/mesh.frag");

    if (!meshShader || !meshShader->IsValid())
        return false;

    // ── Model ────────────────────────────────────────────────────────────────
    m_castleModel = AssetImporter::LoadModel(
        "assets/models/gltf/japanese_castle/scene.gltf");

    if (!m_castleModel.IsValid())
        spdlog::warn("[JapanScene] Castle model failed to load");

    // ── Base Material (shared across all submeshes) ───────────────────────────
    m_castleBaseMaterial = std::make_shared<Material>(meshShader);
    m_castleBaseMaterial->SetVec4("u_TintColor", {1, 1, 1, 1});
    m_castleBaseMaterial->SetFloat("u_Shininess", 64.0f);
    m_castleBaseMaterial->SetFloat("u_SpecularStrength", 0.4f);

    auto whiteFallback = std::make_shared<Texture2D>(
        Texture2D::CreateFallback(200, 200, 200, 255));

    // ── Material Instances (1 per GLTF material) ─────────────────────────────
    for (const ModelMaterialInfo& matInfo : m_castleModel.materials)
    {
        auto inst = std::make_unique<MaterialInstance>(m_castleBaseMaterial);

        std::shared_ptr<Texture2D> tex;
        if (!matInfo.diffusePath.empty())
            tex = AssetImporter::LoadTexture(matInfo.diffusePath, TextureColorSpace::sRGB);

        inst->SetTexture(TextureSlot::Albedo, tex ? tex : whiteFallback);
        m_castleMatInstances.push_back(std::move(inst));
    }

    // ── Render Items (one per submesh) ───────────────────────────────────────
    if (m_castleModel.IsValid())
    {
        const uint32_t subMeshCount = m_castleModel.mesh->SubMeshCount();

        for (uint32_t i = 0; i < subMeshCount; ++i)
        {
            const SubMesh& sm = m_castleModel.mesh->GetSubMesh(i);

            RenderItem item;
            item.meshMulti    = m_castleModel.mesh.get();
            item.subMeshIndex = i;
            item.material     = m_castleMatInstances[sm.materialIndex].get();
            item.transform.SetScale({0.01f, 0.01f, 0.01f});
            item.flags.castShadow    = true;
            item.flags.receiveShadow = true;

            AddObject(item);
        }

        spdlog::info("[JapanScene] Added {} render items", subMeshCount);
    }

    // ── Cherry Blossom Petals (CPU particle system) ──────────────────────────

    // Simple cube flattened into a petal shape
    m_petalMesh = std::make_unique<MeshBuffer>(
        GenerateCube({
            .colorMode = ColorMode::Solid,
            .baseColor = {1.0f, 0.6f, 0.8f} // pink
        }).CreateMeshBuffer());

    constexpr int PETAL_COUNT = 1000;

    for (int i = 0; i < PETAL_COUNT; ++i)
    {
        Petal p;

        // Random spawn in a box above the scene
        p.pos = {
            Rand(-20.0f, 20.0f),
            Rand(5.0f, 40.0f),
            Rand(-20.0f, 20.0f)
        };

        p.speed     = Rand(1.0f, 3.0f);   // falling speed
        p.swayPhase = Rand(0.0f, 6.28f);  // initial phase offset

        RenderItem item;
        item.mesh   = m_petalMesh.get();
        item.shader = meshShader.get();
        item.transform.SetScale({0.1f, 0.02f, 0.1f});
        item.transform.SetTranslation(p.pos);

        item.flags.castShadow    = false; // too many → expensive
        item.flags.receiveShadow = false;

        p.idx = AddObject(item);
        m_petals.push_back(p);
    }

    // ── Camera ───────────────────────────────────────────────────────────────
    Camera cam;
    cam.SetPosition({19.0f, 2.0f, 8.0f});
    cam.SetOrientation(-150.0f, 32.0f);
    SetCamera(cam);

    SetClearColor({0.0f, 0.0f, 0.0f, 1.0f}); // black background

    // ── Lighting ─────────────────────────────────────────────────────────────
    SetAmbientLight({0.18f, 0.20f, 0.24f}, 0.25f);

    auto& lights = GetLights();

    // Main sunlight
    lights.SetDirectionalLight(
        DirectionalLightBuilder()
            .Direction({-0.35f, -1.0f, -0.25f})
            .Color({1.0f, 0.98f, 0.92f})
            .Intensity(1.4f)
            .CastShadow(true)
            .ShadowResolution(2048, 2048)
            .Name("JapanSun")
            .Build());

    // Helper lambda to reduce repetition for point lights
    auto AddLight = [&](glm::vec3 pos, glm::vec3 color, float intensity, const char* name)
    {
        lights.AddPointLight(
            PointLightBuilder()
                .Position(pos)
                .Color(color)
                .Intensity(intensity)
                .Radius(20.0f)
                .Name(name)
                .Build());
    };

    // Scene lighting accents
    AddLight({ 9.18f, 5.58f,  9.12f}, {1,0,0}, 10, "Red 1");
    AddLight({ 9.24f, 5.70f, -9.28f}, {1,0,0}, 10, "Red 2");

    AddLight({ 5.66f,14.65f,  5.61f}, {1.0f,0.47f,0.47f}, 10, "Pink 1");
    AddLight({ 5.59f,14.63f, -5.64f}, {1.0f,0.47f,0.47f}, 10, "Pink 2");
    AddLight({-5.56f,14.69f,  5.62f}, {1.0f,0.47f,0.47f}, 10, "Pink 3");

    AddLight({ 9.08f, 5.48f,  9.13f}, {1,1,1}, 3, "White 1");
    AddLight({ 9.11f, 5.44f, -9.16f}, {1,1,1}, 3, "White 2");

    AddLight({19.86f,-0.10f, -3.21f}, {1.0f,0.73f,0.01f}, 3, "Warm 1");
    AddLight({19.77f,-0.01f,  3.12f}, {1.0f,0.73f,0.01f}, 3, "Warm 2");

    spdlog::info("[JapanScene] Setup complete");
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Update
// ─────────────────────────────────────────────────────────────────────────────
void JapanScene::OnUpdate(float deltaTime, KeyboardInput& input, MouseInput& mouse)
{
    // ── Cherry blossom animation ─────────────────────────────────────────────
    for (auto& p : m_petals)
    {
        // Vertical fall
        p.pos.y -= p.speed * deltaTime;

        // Wind drift (sinusoidal motion)
        p.swayPhase += deltaTime;
        p.pos.x += std::sin(p.swayPhase) * 0.3f * deltaTime;
        p.pos.z += std::cos(p.swayPhase * 0.7f) * 0.2f * deltaTime;

        // Respawn above a scene when hitting ground
        if (p.pos.y < 0.0f)
        {
            p.pos = {
                Rand(-20.0f, 20.0f),
                Rand(10.0f, 20.0f),
                Rand(-20.0f, 20.0f)
            };
        }

        auto& t = GetObject(p.idx).transform;
        t.SetTranslation(p.pos);

        // Subtle spin
        t.SetRotation(glm::angleAxis(p.swayPhase, glm::vec3(0, 1, 0)));
    }

    // ── Camera + player controller ───────────────────────────────────────────
    Camera& cam = GetCamera();

    if (m_playerCubeIdx >= 0)
        GetObject(m_playerCubeIdx).flags.visible =
            (cam.GetMode() != CameraMode::FirstPerson);

    glm::vec3 moveDirXZ;
    UpdateStandardCameraAndPlayer(deltaTime, input, mouse,
                                  m_playerPosition, moveDirXZ, 0.7f);

    // Align the player with a movement direction
    if (m_playerCubeIdx >= 0)
    {
        auto& t = GetObject(m_playerCubeIdx).transform;
        t.SetTranslation(m_playerPosition);

        if (glm::length(moveDirXZ) > 0.001f)
        {
            const glm::vec3 d = glm::normalize(moveDirXZ);
            const float yaw = std::atan2(d.x, d.z);
            t.SetRotation(glm::angleAxis(yaw, glm::vec3(0, 1, 0)));
        }
    }
}