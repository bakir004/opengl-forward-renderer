#include "JapanScene.h"
#include "core/Camera.h"
#include "core/IInputProvider.h"
#include "core/Primitives.h"
#include "core/Texture2D.h"
#include "core/Skybox.h"
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
static float Rand(float min, float max) {
    return min + (max - min) * ((float) (rand() % 100) / 100.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Setup
// Configures all models, materials, particles, and lighting for the Japan scene.
// ─────────────────────────────────────────────────────────────────────────────
bool JapanScene::Setup() {
    spdlog::info("[JapanScene] Setting up");
    SetSceneName("Japan"); // Identifies the scene in the UI/logs

    // ── Shader ───────────────────────────────────────────────────────────────
    // Load the standard PBR mesh shader used for both the castle and the player.
    auto meshShader = AssetImporter::LoadShader(
        "assets/shaders/mesh.vert",
        "assets/shaders/mesh.frag");

    if (!meshShader || !meshShader->IsValid())
        return false; // Critical failure if shaders don't load

    // ── Castle Model ─────────────────────────────────────────────────────────
    // Load the main environment model.
    m_castleModel = AssetImporter::LoadModel(
        "assets/models/gltf/japanese_castle/scene.gltf");

    if (!m_castleModel.IsValid())
        spdlog::warn("[JapanScene] Castle model failed to load");

    // ── Castle Base Material ─────────────────────────────────────────────────
    // Create a base template material that defines the shader and default tint.
    m_castleBaseMaterial = std::make_shared<Material>(meshShader);
    m_castleBaseMaterial->SetVec4("u_TintColor", {1, 1, 1, 1});

    // Create a solid gray texture to use when a specific map (like albedo) is missing.
    auto whiteFallback = std::make_shared<Texture2D>(
        Texture2D::CreateFallback(200, 200, 200, 255));

    // ── Castle Material Instances ───────────────────────────────────────────
    // For every material defined in the GLTF, create a unique MaterialInstance.
    // This allows each part of the castle to have its own textures and PBR values.
    for (const ModelMaterialInfo &matInfo: m_castleModel.materials) {
        auto inst = std::make_unique<MaterialInstance>(m_castleBaseMaterial);
        inst->SetName(matInfo.name);

        // Load and bind the Albedo (diffuse) map
        std::shared_ptr<Texture2D> tex;
        if (!matInfo.diffusePath.empty())
            tex = AssetImporter::LoadTexture(matInfo.diffusePath, TextureColorSpace::sRGB);

        inst->SetTexture(TextureSlot::Albedo, tex ? tex : whiteFallback);

        // Load and bind the Normal map
        if (!matInfo.normalPath.empty()) {
            auto normalTex = AssetImporter::LoadTexture(matInfo.normalPath, TextureColorSpace::Linear);
            if (normalTex) inst->SetTexture(TextureSlot::Normal, normalTex);
        }

        // Load and bind the Metallic-Roughness map (GLTF packed format)
        if (!matInfo.metallicRoughnessPath.empty()) {
            auto mrTex = AssetImporter::LoadTexture(matInfo.metallicRoughnessPath, TextureColorSpace::Linear);
            if (mrTex) {
                inst->SetTexture(TextureSlot::Metallic, mrTex);
                inst->SetTexture(TextureSlot::Roughness, mrTex);
            }
        }

        // Load and bind the Ambient Occlusion map
        if (!matInfo.aoPath.empty()) {
            auto aoTex = AssetImporter::LoadTexture(matInfo.aoPath, TextureColorSpace::Linear);
            if (aoTex) inst->SetTexture(TextureSlot::AO, aoTex);
        }

        // Load and bind the Emissive map
        if (!matInfo.emissivePath.empty()) {
            auto emissiveTex = AssetImporter::LoadTexture(matInfo.emissivePath, TextureColorSpace::sRGB);
            if (emissiveTex) inst->SetTexture(TextureSlot::Emissive, emissiveTex);
        }

        if (!matInfo.specularGlossinessPath.empty()) {
            auto sgTex = AssetImporter::LoadTexture(matInfo.specularGlossinessPath, TextureColorSpace::sRGB);
            if (sgTex) inst->SetTexture(TextureSlot::SpecularGlossiness, sgTex);
        }

        // Apply physical properties extracted from the GLTF file
        inst->SetVec3("u_AlbedoColor", matInfo.albedoColor);
        inst->SetFloat("u_MetallicValue", matInfo.metallicValue);
        inst->SetFloat("u_RoughnessValue", matInfo.roughnessValue);
        inst->SetBool("u_IsSpecularGlossiness", matInfo.isSpecularGlossiness);
        if (matInfo.isSpecularGlossiness) {
            inst->SetVec3("u_SpecularFactor", matInfo.specularFactor);
            inst->SetFloat("u_GlossinessFactor", matInfo.glossinessFactor);
        }
        inst->SetVec3("u_EmissiveColor", matInfo.emissiveColor);
        inst->SetFloat("u_NormalScale", matInfo.normalScale);

        m_castleMatInstances.push_back(std::move(inst));
    }

    // ── Petal Model (Primitive path) ─────────────────────────────────────────
    auto petalData = GenerateCube({.colorMode = ColorMode::Solid, .baseColor = {1.0f, 0.75f, 0.79f}}); // Pinkish
    m_primitivePetalMesh = std::make_unique<MeshBuffer>(petalData.CreateMeshBuffer());

    m_petalBaseMaterial = std::make_shared<Material>(meshShader);
    auto inst = std::make_unique<MaterialInstance>(m_petalBaseMaterial);
    inst->SetName("PrimitivePetalMaterial");
    inst->SetVec3("u_AlbedoColor", {1.0f, 0.75f, 0.79f});
    inst->SetFloat("u_RoughnessValue", 0.8f);
    inst->SetTexture(TextureSlot::Albedo, whiteFallback);
    m_petalMatInstances.push_back(std::move(inst));

    // ── Moon Model ───────────────────────────────────────────────────────────
    m_moonModel = AssetImporter::LoadModel(
        "assets/models/gltf/moon/scene.gltf");

    if (!m_moonModel.IsValid())
        spdlog::warn("[JapanScene] Moon model failed to load");

    // ── Skybox ──────────────────────────────────────────────────────────────
    // Load the test skybox textures from the assets folder.
    std::vector<std::string> skyboxFaces = {
        "assets/skybox/Moody/vz_moody_right.png",   // px = positive X (right)
        "assets/skybox/Moody/vz_moody_left.png",    // nx = negative X (left)
        "assets/skybox/Moody/vz_moody_up.png",      // py = positive Y (up)
        "assets/skybox/Moody/vz_moody_down.png",    // ny = negative Y (down)
        "assets/skybox/Moody/vz_moody_front.png",   // pz = positive Z (front)
        "assets/skybox/Moody/vz_moody_back.png",    // nz = negative Z (back)
    };
    auto skybox = std::make_shared<Skybox>(skyboxFaces);
    SetSkybox(skybox);

    m_moonBaseMaterial = std::make_shared<Material>(meshShader);
    m_moonBaseMaterial->SetVec4("u_TintColor", {1.0f, 1.0f, 1.0f, 1.0f});

    for (const ModelMaterialInfo &matInfo: m_moonModel.materials) {
        auto inst = std::make_unique<MaterialInstance>(m_moonBaseMaterial);
        inst->SetName(matInfo.name);

        std::shared_ptr<Texture2D> tex;
        if (!matInfo.diffusePath.empty())
            tex = AssetImporter::LoadTexture(matInfo.diffusePath, TextureColorSpace::sRGB);
        inst->SetTexture(TextureSlot::Albedo, tex ? tex : whiteFallback);

        if (!matInfo.normalPath.empty()) {
            auto normalTex = AssetImporter::LoadTexture(matInfo.normalPath, TextureColorSpace::Linear);
            if (normalTex) inst->SetTexture(TextureSlot::Normal, normalTex);
        }

        if (!matInfo.metallicRoughnessPath.empty()) {
            auto mrTex = AssetImporter::LoadTexture(matInfo.metallicRoughnessPath, TextureColorSpace::Linear);
            if (mrTex) {
                inst->SetTexture(TextureSlot::Metallic, mrTex);
                inst->SetTexture(TextureSlot::Roughness, mrTex);
            }
        }

        inst->SetVec3("u_AlbedoColor", matInfo.albedoColor);
        inst->SetFloat("u_MetallicValue", matInfo.metallicValue);
        inst->SetFloat("u_RoughnessValue", matInfo.roughnessValue);
        inst->SetBool("u_IsSpecularGlossiness", matInfo.isSpecularGlossiness);
        if (matInfo.isSpecularGlossiness) {
            inst->SetVec3("u_SpecularFactor", matInfo.specularFactor);
            inst->SetFloat("u_GlossinessFactor", matInfo.glossinessFactor);
        }

        // The moon should glow. We use its diffuse texture as the emissive source
        // to keep crater details, with a slight warm tint and increased strength.
        if (tex) {
            inst->SetTexture(TextureSlot::Emissive, tex);
            inst->SetVec3("u_EmissiveColor", {0.6f, 0.0f, 0.0f});
            inst->SetFloat("u_EmissiveStrength", 2.0f);
        } else {
            inst->SetVec3("u_EmissiveColor", matInfo.emissiveColor);
            inst->SetFloat("u_EmissiveStrength", 2.0f);
        }

        if (!matInfo.specularGlossinessPath.empty()) {
            auto sgTex = AssetImporter::LoadTexture(matInfo.specularGlossinessPath, TextureColorSpace::sRGB);
            if (sgTex) inst->SetTexture(TextureSlot::SpecularGlossiness, sgTex);
        }

        m_moonMatInstances.push_back(std::move(inst));
    }

    // ── Moon Render Item ─────────────────────────────────────────────────────
    if (m_moonModel.IsValid()) {
        const uint32_t subMeshCount = m_moonModel.mesh->SubMeshCount();
        for (uint32_t i = 0; i < subMeshCount; ++i) {
            const SubMesh &sm = m_moonModel.mesh->GetSubMesh(i);
            RenderItem item;
            item.meshMulti = m_moonModel.mesh.get();
            item.subMeshIndex = i;
            if (sm.materialIndex < m_moonMatInstances.size())
                item.material = m_moonMatInstances[sm.materialIndex].get();
            else
                item.material = m_moonMatInstances.empty() ? nullptr : m_moonMatInstances[0].get();

            // Position the moon far away in the sky
            item.transform.SetTranslation({-120.0f, 250.0f, -200.0f});
            item.transform.SetScale({0.4f, 0.4f, 0.4f});
            item.flags.castShadow = false;
            item.flags.receiveShadow = false;

            AddObject(item);
        }
    }

    // ── Castle Render Items ──────────────────────────────────────────────────
    // Create a RenderItem for every submesh in the castle model.
    // We scale it down because GLTF units might be larger than our scene scale.
    if (m_castleModel.IsValid()) {
        const uint32_t subMeshCount = m_castleModel.mesh->SubMeshCount();

        for (uint32_t i = 0; i < subMeshCount; ++i) {
            const SubMesh &sm = m_castleModel.mesh->GetSubMesh(i);

            RenderItem item;
            item.meshMulti = m_castleModel.mesh.get();
            item.subMeshIndex = i;
            // Map the submesh to its corresponding material instance
            if (sm.materialIndex < m_castleMatInstances.size())
                item.material = m_castleMatInstances[sm.materialIndex].get();
            else
                item.material = m_castleMatInstances.empty() ? nullptr : m_castleMatInstances[0].get();

            item.transform.SetScale({0.01f, 0.01f, 0.01f});
            item.flags.castShadow = true;
            item.flags.receiveShadow = true;

            AddObject(item);
        }

        spdlog::info("[JapanScene] Added {} render items", subMeshCount);
    }

    // ── Cherry Blossom Petals (CPU particle system) ──────────────────────────
    constexpr int PETAL_COUNT = 1500; // Restored high count for primitive petals

    // Initialize individual petals with random properties for variety.
    for (int i = 0; i < PETAL_COUNT; ++i) {
        Petal p;

        // Random spawn in a box volume above the scene
        p.pos = {
            Rand(-20.0f, 20.0f),
            Rand(5.0f, 40.0f),
            Rand(-20.0f, 20.0f)
        };

        p.speed = Rand(1.0f, 5.0f); // How fast it falls
        p.swayPhase = Rand(0.0f, 6.28f); // Random starting point for the oscillation

        // Random axis and speed for the tumbling rotation
        p.rotAxis = glm::normalize(glm::vec3(
            Rand(-1.0f, 1.0f),
            Rand(-1.0f, 1.0f),
            Rand(-1.0f, 1.0f)
        ));

        p.rotSpeed = Rand(0.5f, 2.0f);
        p.baseScale = Rand(0.05f, 0.15f);

        // Primitive path
        RenderItem item;
        item.mesh = m_primitivePetalMesh.get();
        item.material = m_petalMatInstances[0].get();
        item.transform.SetScale({0.1f, 0.02f, 0.1f});
        item.transform.SetTranslation(p.pos);

        item.flags.castShadow = false;
        item.flags.receiveShadow = false;

        p.idx = AddObject(item);
        m_petals.push_back(p);
    }

    // ── Camera Configuration ─────────────────────────────────────────────────
    Camera cam;
    cam.SetPosition({20.0f, 0.0f, 10.0f});
    cam.SetOrientation(-150.0f, 32.0f);
    SetCamera(cam);
    SetFirstPersonEyeHeight(2.20f); // Adjusts where the camera sits relative to the player position

    SetClearColor({0.0f, 0.0f, 0.0f, 1.0f}); // Black background for high-contrast lighting

    // ── Lighting ─────────────────────────────────────────────────────────────
    // Set up ambient environment light (dark blue-ish for a night/evening feel)
    SetAmbientLight({0.18f, 0.20f, 0.24f}, 0.25f);

    auto &lights = GetLights();

    // Main directional sunlight (acting as moonlight or high-key lighting)
    lights.SetDirectionalLight(
        DirectionalLightBuilder()
        .Direction({-0.35f, -1.0f, -0.25f})
        .Color({1.0f, 0.98f, 0.92f})
        .Intensity(0.5f)
        .CastShadow(true)
        .ShadowResolution(2048, 2048)
        .Name("JapanSun")
        .Build());

    // Helper lambda to reduce point light boilerplate
    auto AddLight = [&](glm::vec3 pos, glm::vec3 color, float intensity, const char *name) {
        lights.AddPointLight(
            PointLightBuilder()
            .Position(pos)
            .Color(color)
            .Intensity(intensity)
            .Radius(20.0f)
            .CastShadow(true)
            .Name(name)
            .Build());
    };

    // Place decorative lights around the scene
    AddLight({9.18f, 5.70f, 9.12f}, {1, 0, 0}, 30, "Floor 0 Red 1");
    AddLight({9.24f, 5.70f, -9.12f}, {1, 0, 0}, 30, "Floor 0 Red 2");
    AddLight({-9.16f, 5.70f, 9.12f}, {1, 0, 0}, 30, "Floor 0 Red 3");
    AddLight({-9.16f, 5.70f, -9.12f}, {1, 0, 0}, 30, "Floor 0 Red 4");

    AddLight({5.66f, 14.65f, 5.61f}, {1.0f, 0.47f, 0.47f}, 10, "Floor 1 Pink 1");
    AddLight({5.59f, 14.63f, -5.64f}, {1.0f, 0.47f, 0.47f}, 10, "Floor 1 Pink 2");
    AddLight({-5.56f, 14.69f, 5.62f}, {1.0f, 0.47f, 0.47f}, 10, "Floor 1 Pink 3");
    AddLight({-5.56f, 14.69f, -5.62f}, {1.0f, 0.47f, 0.47f}, 10, "Floor 1 Pink 3");

    AddLight({9.08f, 5.48f, 9.13f}, {1, 1, 1}, 5, "White 1");
    AddLight({9.11f, 5.44f, -9.16f}, {1, 1, 1}, 5, "White 2");

    AddLight({19.86f, -0.10f, -3.21f}, {1.0f, 0.73f, 0.01f}, 10, "Steps Warm 1");
    AddLight({19.77f, -0.01f, 3.12f}, {1.0f, 0.73f, 0.01f}, 10, "Steps Warm 2");
    //AddLight({12.25f, 3.0f, -3.0f}, {1.0f, 0.73f, 0.01f}, 10, "Steps Warm 3");
    //AddLight({12.25f, 3.0f, 3.0f}, {1.0f, 0.73f, 0.01f}, 10, "Steps Warm 4");

    AddLight({0.00f, 32.00f, 0.00f}, {1.0f, 0.47f, 0.47f}, 100, "TopLight");

    // ── Sekiro Player Model ──────────────────────────────────────────────────
    // Load the character model that represents the user.
    m_sekiroModel = AssetImporter::LoadModel("assets/models/gltf/low-poly_sekiro/scene.gltf");

    m_sekiroMaterial = std::make_shared<Material>(meshShader);
    m_sekiroMaterial->SetVec4("u_TintColor", {1.0f, 1.0f, 1.0f, 1.0f});

    // Sekiro model doesn't use textures, but the shader expects one bound.
    auto whiteFallbackSekiro = std::make_shared<Texture2D>(
        Texture2D::CreateFallback(200, 200, 200, 255));

    // Hardcoded color lookup for materials by name (since this low-poly model uses solid colors)
    static const std::unordered_map<std::string, glm::vec4> kSekiroColors = {
        {"Dark_red", {0.0467f, 0.0301f, 0.0533f, 1.0f}},
        {"Grey", {0.0510f, 0.0510f, 0.0510f, 1.0f}},
        {"light_grey", {0.135f, 0.135f, 0.135f, 1.0f}},
        {"Scarf", {0.800f, 0.490f, 0.274f, 1.0f}},
        {"Face", {0.800f, 0.424f, 0.306f, 1.0f}},
        {"Hair", {0.025f, 0.024f, 0.024f, 1.0f}},
        {"Bandages", {0.482f, 0.262f, 0.153f, 1.0f}},
        {"Metal", {0.393f, 0.393f, 0.393f, 1.0f}},
        {"Bone", {0.471f, 0.325f, 0.118f, 1.0f}},
        {"Bone_light", {0.624f, 0.428f, 0.298f, 1.0f}},
        {"New_Coat", {0.554f, 0.219f, 0.096f, 1.0f}},
        {"Pants", {0.155f, 0.094f, 0.069f, 1.0f}},
        {"material", {0.093f, 0.006f, 0.006f, 1.0f}},
        {"Material.003", {0.058f, 0.035f, 0.022f, 1.0f}},
        {"Black", {0.012f, 0.011f, 0.011f, 1.0f}},
        {"Bronze", {0.285f, 0.264f, 0.157f, 1.0f}},
        {"Sword_red", {0.203f, 0.043f, 0.029f, 1.0f}},
        {"Red_Metal", {0.187f, 0.085f, 0.085f, 1.0f}},
    };

    // Create material instances for each part of the character
    for (const ModelMaterialInfo &matInfo: m_sekiroModel.materials) {
        auto inst = std::make_unique<MaterialInstance>(m_sekiroMaterial);
        inst->SetName(matInfo.name);
        inst->SetTexture(TextureSlot::Albedo, whiteFallbackSekiro);

        inst->SetVec3("u_AlbedoColor", matInfo.albedoColor);
        inst->SetFloat("u_MetallicValue", matInfo.metallicValue);
        inst->SetFloat("u_RoughnessValue", matInfo.roughnessValue);
        inst->SetBool("u_IsSpecularGlossiness", matInfo.isSpecularGlossiness);
        if (matInfo.isSpecularGlossiness) {
            inst->SetVec3("u_SpecularFactor", matInfo.specularFactor);
            inst->SetFloat("u_GlossinessFactor", matInfo.glossinessFactor);
        }
        inst->SetVec3("u_EmissiveColor", matInfo.emissiveColor);

        if (!matInfo.specularGlossinessPath.empty()) {
            auto sgTex = AssetImporter::LoadTexture(matInfo.specularGlossinessPath, TextureColorSpace::sRGB);
            if (sgTex) inst->SetTexture(TextureSlot::SpecularGlossiness, sgTex);
        }

        // Apply our custom color overrides if they match the material name
        auto it = kSekiroColors.find(matInfo.name);
        if (it != kSekiroColors.end())
            inst->SetVec4("u_TintColor", it->second);
        else
            inst->SetVec4("u_TintColor", {1.0f, 1.0f, 1.0f, 1.0f});

        m_sekiroMatInstances.push_back(std::move(inst));
    }

    // Add Sekiro to the scene as a collection of submeshes
    if (m_sekiroModel.IsValid()) {
        const uint32_t subMeshCount = m_sekiroModel.mesh->SubMeshCount();

        for (uint32_t i = 0; i < subMeshCount; ++i) {
            const SubMesh &sm = m_sekiroModel.mesh->GetSubMesh(i);

            RenderItem item;
            item.meshMulti = m_sekiroModel.mesh.get();
            item.subMeshIndex = i;
            if (sm.materialIndex < m_sekiroMatInstances.size())
                item.material = m_sekiroMatInstances[sm.materialIndex].get();
            else
                item.material = m_sekiroMatInstances.empty() ? nullptr : m_sekiroMatInstances[0].get();

            item.transform.SetTranslation(m_playerPosition);
            item.transform.SetScale({0.01f, 0.01f, 0.01f});
            item.flags.castShadow = true;
            item.flags.receiveShadow = true;

            size_t idx = AddObject(item);
            m_sekiroSubMeshIndices.push_back(idx);

            // Keep track of the first submesh as a master handle
            if (m_playerCubeIdx == (size_t) -1)
                m_playerCubeIdx = idx;
        }

        spdlog::info("[JapanScene] Added {} player model submeshes", subMeshCount);
    }

    spdlog::info("[JapanScene] Setup complete");
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Update
// Called once per frame to handle animations, physics, and input.
// ─────────────────────────────────────────────────────────────────────────────
void JapanScene::OnUpdate(float deltaTime, IInputProvider &input) {
    // ── Wind Mechanism ───────────────────────────────────────────────────────
    // Periodically triggers a 'wind gust' that applies a force to the petals.
    m_windTimer += deltaTime;

    if (!m_windActive && m_windTimer > m_windInterval) {
        // Start a new wind gust
        m_windActive = true;
        m_windTimer = 0.0f;

        // Choose a random horizontal direction for the gust
        m_windDir = glm::normalize(glm::vec3(
            Rand(-1.0f, 1.0f),
            0.0f,
            Rand(-1.0f, 1.0f)
        ));

        m_windStrength = Rand(4.0f, 8.0f); // Randomize intensity
    } else if (m_windActive && m_windTimer > m_windDuration) {
        // Gust has finished its duration, return to calm state
        m_windActive = false;
        m_windTimer = 0.0f;
    }

    // ── Cherry Blossom Animation ─────────────────────────────────────────────
    // Updates position and rotation for every petal in the scene.
    for (auto &p: m_petals) {
        // ── Base Motion ──
        // Constant vertical descent
        p.pos.y -= p.speed * deltaTime;

        // Update oscillation phase
        p.swayPhase += deltaTime;

        // Gentle ambient drift (sinusoidal) to simulate air resistance
        p.pos.x += std::sin(p.swayPhase) * 0.3f * deltaTime;
        p.pos.z += std::cos(p.swayPhase * 0.7f) * 0.2f * deltaTime;

        // ── Wind Gust Effect ──
        float windFactor = 0.0f;
        if (m_windActive) {
            // Smoothly ramp the wind force up and down over the duration (sine curve)
            float t = m_windTimer / m_windDuration;
            windFactor = std::sin(t * 3.1415926f);
        }

        // Apply height-based scaling so petals higher up feel more wind
        float heightFactor = glm::clamp(p.pos.y / 20.0f, 0.2f, 1.0f);

        // Add wind displacement to the current position
        p.pos += m_windDir * m_windStrength * windFactor * heightFactor * deltaTime;

        // ── Respawn ──
        // If a petal falls below the ground, teleport it back to the top
        if (p.pos.y < 0.0f) {
            p.pos = {
                Rand(-20.0f, 20.0f),
                Rand(10.0f, 20.0f),
                Rand(-20.0f, 20.0f)
            };
        }

        // ── Final Transform Update ──
        auto &t = GetObject(p.idx).transform;

        // Simulate 'fluttering' by oscillating the scale slightly
        float flutter = std::sin(p.swayPhase * 5.0f) * 0.2f + 1.0f;

        // Apply flattened non-uniform scale (X and Z are wider than Y)
        glm::vec3 scale = glm::vec3(p.baseScale * flutter, p.baseScale * flutter * 0.2f, p.baseScale * flutter);

        t.SetScale(scale);

        // Apply constant rotation around the petal's unique random axis
        t.SetRotation(glm::angleAxis(
            p.swayPhase * p.rotSpeed,
            p.rotAxis
        ));

        // Update the actual scene object position
        t.SetTranslation(p.pos);
    }


    // ── Camera + Player Controller ───────────────────────────────────────────
    Camera &cam = GetCamera();

    // In First-Person mode, we hide the character model so the user doesn't see 
    // the inside of their own head. In other modes (Orbit/Free), we show it.
    if (m_sekiroModel.IsValid() && m_playerCubeIdx != (size_t) -1)
        GetObject(m_playerCubeIdx).flags.visible =
                (cam.GetMode() != CameraMode::FirstPerson);

    // Standard camera logic: handles WASD movement and mouse looking.
    // m_playerPosition is updated by this call.
    glm::vec3 moveDirXZ;
    UpdateStandardCameraAndPlayer(deltaTime, input,
                                  m_playerPosition, moveDirXZ, 2.0f);

    // Synchronize all submeshes of the Sekiro model with the player position 
    // and make them face the direction of travel.
    for (size_t idx: m_sekiroSubMeshIndices) {
        RenderItem &item = GetObject(idx);
        item.flags.visible = (cam.GetMode() != CameraMode::FirstPerson);

        auto &t = item.transform;
        t.SetTranslation(m_playerPosition);

        // Rotate the model to face the move direction (if we are moving)
        if (glm::length(moveDirXZ) > 0.001f) {
            const glm::vec3 d = glm::normalize(moveDirXZ);
            const float yawRad = std::atan2(d.x, d.z);
            t.SetRotation(glm::angleAxis(yawRad, glm::vec3(0.0f, 1.0f, 0.0f)));
        }
    }
}
