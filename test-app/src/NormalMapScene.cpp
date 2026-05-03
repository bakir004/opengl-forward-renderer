// NormalMapScene.cpp
// T9 — Normal mapping demo scene.
//
// Layout:
//   • A wooden bench centred in the scene, fully PBR-textured (albedo + normal + roughnessMetallic).
//   • An avocado sitting on top of the bench — albedo + normal + roughnessMetallic.
//   • A lantern standing to the right of the bench — albedo + normal + roughnessMetallic + emissive.
//
// The scene is lit with a directional key light (~30° elevation) to make normal-mapped
// surface detail visible, plus a warm point light inside the lantern.

#include "NormalMapScene.h"

#include "assets/AssetImporter.h"
#include "core/Material.h"
#include "core/MeshBuffer.h"
#include "core/ShaderProgram.h"
#include "core/Texture2D.h"
#include "scene/LightBuilder.h"
#include "scene/RenderItem.h"

#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

bool NormalMapScene::Setup()
{
    spdlog::info("[NormalMapScene] Setting up");
    SetSceneName("Normal Map Demo");

    // ── Shader ───────────────────────────────────────────────────────────────
    auto shader = AssetImporter::LoadShader(
        "assets/shaders/mesh.vert",
        "assets/shaders/mesh.frag");

    if (!shader || !shader->IsValid()) {
        spdlog::error("[NormalMapScene] Mesh shader failed to load");
        return false;
    }

    // =========================================================================
    // BENCH — has: baseColor + normal + metallicRoughness
    // =========================================================================
    m_bench = AssetImporter::Import<MeshBuffer>("assets/models/gltf/bench/scene.gltf");
    if (!m_bench)
        spdlog::warn("[NormalMapScene] Bench mesh failed to load");

    m_benchMat = std::make_shared<Material>(shader);
    m_benchMat->SetTexture(TextureSlot::Albedo,
        AssetImporter::LoadTexture(
            "assets/models/gltf/bench/MesaBanco.Comedor_baseColor.png",
            TextureColorSpace::sRGB));
    m_benchMat->SetTexture(TextureSlot::Normal,
        AssetImporter::LoadTexture(
            "assets/models/gltf/bench/MesaBanco.Comedor_normal.png",
            TextureColorSpace::Linear));
    // The bench metallicRoughness texture packs: G=roughness, B=metallic
    // We bind it to the Roughness slot and read .r for metallic via MetallicMap too.
    // The PBR shader samples MetallicMap.r and RoughnessMap.r independently,
    // so we bind the same packed texture to both slots — harmless, each reads
    // the relevant channel only.
    auto benchRoughMetal = AssetImporter::LoadTexture(
        "assets/models/gltf/bench/MesaBanco.Comedor_metallicRoughness.png",
        TextureColorSpace::Linear);
    m_benchMat->SetTexture(TextureSlot::Roughness, benchRoughMetal);
    m_benchMat->SetTexture(TextureSlot::Metallic,  benchRoughMetal);
    m_benchMat->SetVec4("u_TintColor", {1.0f, 1.0f, 1.0f, 1.0f});

    if (m_bench) {
        auto inst = std::make_unique<MaterialInstance>(m_benchMat);
        RenderItem item;
        item.mesh      = m_bench.get();
        item.material  = inst.get();
        item.transform.SetTranslation({0.0f, 0.0f, 0.0f});
        item.transform.SetScale({1.0f, 1.0f, 1.0f});
        item.flags.castShadow    = true;
        item.flags.receiveShadow = true;
        AddObject(item);
        m_instances.push_back(std::move(inst));
    }

    // =========================================================================
    // AVOCADO — has: baseColor + normal + roughnessMetallic
    // =========================================================================
    m_avocado = AssetImporter::Import<MeshBuffer>("assets/models/gltf/avocado/Avocado.gltf");
    if (!m_avocado)
        spdlog::warn("[NormalMapScene] Avocado mesh failed to load");

    m_avocadoMat = std::make_shared<Material>(shader);
    m_avocadoMat->SetTexture(TextureSlot::Albedo,
        AssetImporter::LoadTexture(
            "assets/models/gltf/avocado/Avocado_baseColor.png",
            TextureColorSpace::sRGB));
    m_avocadoMat->SetTexture(TextureSlot::Normal,
        AssetImporter::LoadTexture(
            "assets/models/gltf/avocado/Avocado_normal.png",
            TextureColorSpace::Linear));
    auto avocadoRoughMetal = AssetImporter::LoadTexture(
        "assets/models/gltf/avocado/Avocado_roughnessMetallic.png",
        TextureColorSpace::Linear);
    m_avocadoMat->SetTexture(TextureSlot::Roughness, avocadoRoughMetal);
    m_avocadoMat->SetTexture(TextureSlot::Metallic,  avocadoRoughMetal);
    m_avocadoMat->SetVec4("u_TintColor", {1.0f, 1.0f, 1.0f, 1.0f});

    if (m_avocado) {
        auto inst = std::make_unique<MaterialInstance>(m_avocadoMat);
        RenderItem item;
        item.mesh     = m_avocado.get();
        item.material = inst.get();
        item.transform.SetTranslation({0.0f, 0.58f, 0.0f});
        item.transform.SetScale({18.0f, 18.0f, 18.0f});
        item.flags.castShadow    = true;
        item.flags.receiveShadow = true;
        AddObject(item);
        m_instances.push_back(std::move(inst));
    }

    // =========================================================================
    // LANTERN — has: baseColor + normal + roughnessMetallic + emissive
    // =========================================================================
    m_lantern = AssetImporter::Import<MeshBuffer>("assets/models/gltf/lantern/Lantern.gltf");
    if (!m_lantern)
        spdlog::warn("[NormalMapScene] Lantern mesh failed to load");

    m_lanternMat = std::make_shared<Material>(shader);
    m_lanternMat->SetTexture(TextureSlot::Albedo,
        AssetImporter::LoadTexture(
            "assets/models/gltf/lantern/Lantern_baseColor.png",
            TextureColorSpace::sRGB));
    m_lanternMat->SetTexture(TextureSlot::Normal,
        AssetImporter::LoadTexture(
            "assets/models/gltf/lantern/Lantern_normal.png",
            TextureColorSpace::Linear));
    auto lanternRoughMetal = AssetImporter::LoadTexture(
        "assets/models/gltf/lantern/Lantern_roughnessMetallic.png",
        TextureColorSpace::Linear);
    m_lanternMat->SetTexture(TextureSlot::Roughness, lanternRoughMetal);
    m_lanternMat->SetTexture(TextureSlot::Metallic,  lanternRoughMetal);
    m_lanternMat->SetTexture(TextureSlot::Emissive,
        AssetImporter::LoadTexture(
            "assets/models/gltf/lantern/Lantern_emissive.png",
            TextureColorSpace::sRGB));
    m_lanternMat->SetVec3("u_EmissiveColor", {1.4f, 1.0f, 0.6f});
    m_lanternMat->SetVec4("u_TintColor", {1.0f, 1.0f, 1.0f, 1.0f});

    if (m_lantern) {
        auto inst = std::make_unique<MaterialInstance>(m_lanternMat);
        RenderItem item;
        item.mesh     = m_lantern.get();
        item.material = inst.get();
        item.transform.SetTranslation({1.5f, 0.0f, -1.0f});
        item.transform.SetScale({0.35f, 0.35f, 0.35f});
        item.flags.castShadow    = true;
        item.flags.receiveShadow = true;
        AddObject(item);
        m_instances.push_back(std::move(inst));
    }

    Camera cam;
    cam.SetPosition({3.5f, 1.5f, 3.5f});
    cam.SetOrientation(-135.0f, -20.0f);
    SetCamera(cam);
    SetFirstPersonEyeHeight(1.7f);

    // Very dark clear colour — mostly hidden by the room walls.
    SetClearColor({0.04f, 0.04f, 0.05f, 1.0f});

    // ── Lighting ─────────────────────────────────────────────────────────────
    // Key design: ONE strong directional light at ~30° elevation from the side.
    // This is the ideal angle to make normal map surface detail pop.
    // A nearly-frontal or top-down light would wash out the normal map effect.
    SetAmbientLight({0.08f, 0.09f, 0.11f}, 0.30f);

    auto& lights = GetLights();
    lights.GetPointLights().clear();
    lights.GetSpotLights().clear();

    // Main key light — comes from upper-left at ~30° elevation
    lights.SetDirectionalLight(
        DirectionalLightBuilder()
            .Direction({-0.60f, -0.55f, -0.58f})   // side-angle, not top-down
            .Color({1.0f, 0.97f, 0.90f})            // slightly warm white
            .Intensity(8.0f)
            .CastShadow(false)                       // keep it simple for this demo
            .Name("KeyLight")
            .Build());

    // Warm fill from inside the lantern — mirrors RenderItem translation (1.5, 0.0, -1.0).
    lights.AddPointLight(
        PointLightBuilder()
            .Position({1.5f, 0.6f, -1.0f})
            .Color({1.0f, 0.70f, 0.30f})
            .Intensity(4.0f)
            .Radius(8.0f)
            .Name("LanternGlow")
            .Build());

    spdlog::info("[NormalMapScene] Setup complete — bench, avocado, lantern ready");
    return true;
}

void NormalMapScene::OnUpdate(float deltaTime, IInputProvider& input)
{
    // Camera anchor at bench height so orbit stays focused on the scene centrepiece.
    glm::vec3 moveDirXZ{0.0f};
    UpdateStandardCameraAndPlayer(deltaTime, input, m_cameraAnchor, moveDirXZ, 0.0f);
}
