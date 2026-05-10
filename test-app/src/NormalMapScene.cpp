// NormalMapScene.cpp
// T9 — Normal mapping demo scene.
//
// Layout:
//   • A wooden kitchen environment (multi-material GLTF, 29 mats / 98 submeshes) — full PBR.
//   • A wooden bench centred in the scene, fully PBR-textured (albedo + normal + roughnessMetallic).
//   • An avocado sitting on top of the bench — albedo + normal + roughnessMetallic.
//   • A lantern standing to the right of the bench — albedo + normal + roughnessMetallic + emissive.
//
// The scene is lit with a directional key light (~30° elevation) to make normal-mapped
// surface detail visible, plus a warm point light inside the lantern.

#include "NormalMapScene.h"

#include "assets/AssetImporter.h"
#include "assets/ModelData.h"
#include "core/Material.h"
#include "core/Mesh.h"
#include "core/MeshBuffer.h"
#include "core/ShaderProgram.h"
#include "core/SubMesh.h"
#include "core/Texture2D.h"
#include "scene/LightBuilder.h"
#include "scene/RenderItem.h"

#include <glm/glm.hpp>
#include <spdlog/spdlog.h>
#include <imgui.h>

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
        item.transform.SetTranslation({3.0f, 0.0f, 8.0f});
        item.transform.SetScale({2.0f, 2.0f, 2.0f});
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
        item.transform.SetTranslation({2.0f, 2.0f, 3.0f});
        item.transform.SetScale({5.0f, 5.0f, 5.0f});
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
        item.transform.SetTranslation({2.0f, -0.1f, 4.25f});
        item.transform.SetScale({0.2f, 0.2f, 0.2f});
        item.flags.castShadow    = true;
        item.flags.receiveShadow = true;
        AddObject(item);
        m_instances.push_back(std::move(inst));
    }

    // =========================================================================
    // WOODEN KITCHEN — multi-material GLTF (29 materials, 98 submeshes)
    //
    // Each GLTF material gets its own Material object with full PBR textures
    // (baseColor + normal + metallicRoughness, emissive where present).
    // Each submesh is submitted as a separate RenderItem so the renderer can
    // bind the correct MaterialInstance per draw call.
    //
    // Texture convention (same as the rest of the scene):
    //   Albedo / emissive  → sRGB
    //   Normal / MR        → Linear
    // =========================================================================
    static const char* K = "assets/models/gltf/wooden_kitchen/";
    static const char* T = "assets/models/gltf/wooden_kitchen/textures/";

    m_kitchen = AssetImporter::LoadModel("assets/models/gltf/wooden_kitchen/scene.gltf");
    if (!m_kitchen.IsValid())
        spdlog::warn("[NormalMapScene] Wooden kitchen failed to load");

    // Helper lambda — load a texture or return nullptr if path is empty.
    auto LoadTex = [&](const std::string& path, TextureColorSpace cs)
        -> std::shared_ptr<Texture2D>
    {
        if (path.empty()) return nullptr;
        return AssetImporter::LoadTexture(path, cs);
    };

    // Helper — create a Material, set all PBR textures, push into m_kitchenMats.
    auto MakeMat = [&](const std::string& bc, const std::string& nrm,
                       const std::string& mr,  const std::string& em) -> Material*
    {
        auto mat = std::make_shared<Material>(shader);
        if (auto t = LoadTex(bc,  TextureColorSpace::sRGB))   mat->SetTexture(TextureSlot::Albedo,    t);
        if (auto t = LoadTex(nrm, TextureColorSpace::Linear)) mat->SetTexture(TextureSlot::Normal,    t);
        if (auto t = LoadTex(mr,  TextureColorSpace::Linear)) {
            mat->SetTexture(TextureSlot::Roughness, t);
            mat->SetTexture(TextureSlot::Metallic,  t);
        }
        if (auto t = LoadTex(em,  TextureColorSpace::sRGB))   mat->SetTexture(TextureSlot::Emissive,  t);
        mat->SetVec4("u_TintColor", {1.0f, 1.0f, 1.0f, 1.0f});
        m_kitchenMats.push_back(mat);
        return mat.get();
    };

    // -------------------------------------------------------------------------
    // Per-material setup — order MUST match the GLTF material indices [0..28]
    // so that SubMesh::materialIndex correctly indexes m_kitchenMats.
    // -------------------------------------------------------------------------
    // [00] None.007
    MakeMat(std::string(T)+"None.007_baseColor.png",
            std::string(T)+"None.007_normal.png",
            std::string(T)+"None.007_metallicRoughness.png", "");
    // [01] None
    MakeMat(std::string(T)+"None_baseColor.png",
            std::string(T)+"None_normal.png",
            std::string(T)+"None_metallicRoughness.png", "");
    // [02] None.002
    MakeMat(std::string(T)+"None.002_baseColor.png",
            std::string(T)+"None.002_normal.png",
            std::string(T)+"None.002_metallicRoughness.png", "");
    // [03] None.003
    MakeMat(std::string(T)+"None.003_baseColor.png",
            std::string(T)+"None.003_normal.png",
            std::string(T)+"None.003_metallicRoughness.png", "");
    // [04] None.005 (shares None.002 textures per GLTF definition)
    MakeMat(std::string(T)+"None.002_baseColor.png",
            std::string(T)+"None.002_normal.png",
            std::string(T)+"None.002_metallicRoughness.png", "");
    // [05] cupboardup
    MakeMat(std::string(T)+"cupboardup_baseColor.png",
            std::string(T)+"cupboardup_normal.png",
            std::string(T)+"cupboardup_metallicRoughness.png", "");
    // [06] cupboardups
    MakeMat(std::string(T)+"cupboardups_baseColor.png",
            std::string(T)+"cupboardups_normal.png",
            std::string(T)+"cupboardups_metallicRoughness.png", "");
    // [07] cupboardupssss (uses cupboardups normal per GLTF)
    MakeMat(std::string(T)+"cupboardupssss_baseColor.png",
            std::string(T)+"cupboardups_normal.png",
            std::string(T)+"cupboardupssss_metallicRoughness.png", "");
    // [08] cupboardupsbok
    MakeMat(std::string(T)+"cupboardupsbok_baseColor.png",
            std::string(T)+"cupboardupsbok_normal.png",
            std::string(T)+"cupboardupsbok_metallicRoughness.png", "");
    // [09] cupboardupsbokss (uses cupboardupsbok normal per GLTF)
    MakeMat(std::string(T)+"cupboardupsbokss_baseColor.png",
            std::string(T)+"cupboardupsbok_normal.png",
            std::string(T)+"cupboardupsbokss_metallicRoughness.png", "");
    // [10] fridge
    MakeMat(std::string(T)+"fridge_baseColor.png",
            std::string(T)+"fridge_normal.png",
            std::string(T)+"fridge_metallicRoughness.png", "");
    // [11] None.001
    MakeMat(std::string(T)+"None.001_baseColor.png",
            std::string(T)+"None.001_normal.png",
            std::string(T)+"None.001_metallicRoughness.png", "");
    // [12] material
    MakeMat(std::string(T)+"material_baseColor.png",
            std::string(T)+"material_normal.png",
            std::string(T)+"material_metallicRoughness.png", "");
    // [13] krujka.001
    MakeMat(std::string(T)+"krujka.001_baseColor.png",
            std::string(T)+"krujka.001_normal.png",
            std::string(T)+"krujka.001_metallicRoughness.png", "");
    // [14] krujka (shares krujka.001 normal+MR per GLTF)
    MakeMat(std::string(T)+"krujka_baseColor.png",
            std::string(T)+"krujka.001_normal.png",
            std::string(T)+"krujka.001_metallicRoughness.png", "");
    // [15] Zavarka.001
    MakeMat(std::string(T)+"Zavarka.001_baseColor.png",
            std::string(T)+"Zavarka.001_normal.png",
            std::string(T)+"Zavarka.001_metallicRoughness.png", "");
    // [16] Zavarka.002 (shares Zavarka.001 normal per GLTF)
    MakeMat(std::string(T)+"Zavarka.002_baseColor.png",
            std::string(T)+"Zavarka.001_normal.png",
            std::string(T)+"Zavarka.001_metallicRoughness.png", "");
    // [17] Zavarka (shares Zavarka.002 bc + Zavarka.001 normal per GLTF)
    MakeMat(std::string(T)+"Zavarka.002_baseColor.png",
            std::string(T)+"Zavarka.001_normal.png",
            std::string(T)+"Zavarka_metallicRoughness.png", "");
    // [18] material_18
    MakeMat(std::string(T)+"material_18_baseColor.png",
            std::string(T)+"material_18_normal.png",
            std::string(T)+"material_18_metallicRoughness.png", "");
    // [19] material_19 — has emissive (lights/screen)
    MakeMat(std::string(T)+"material_19_baseColor.png",
            std::string(T)+"material_19_normal.png",
            std::string(T)+"material_19_metallicRoughness.png",
            std::string(T)+"material_19_emissive.png");
    // [20] material_20 — no textures (fallback white)
    MakeMat("", "", "", "");
    // [21] board
    MakeMat(std::string(T)+"board_baseColor.png",
            std::string(T)+"board_normal.png",
            std::string(T)+"board_metallicRoughness.png", "");
    // [22] plate
    MakeMat(std::string(T)+"plate_baseColor.png",
            std::string(T)+"plate_normal.png",
            std::string(T)+"plate_metallicRoughness.png", "");
    // [23] plita (cooktop)
    MakeMat(std::string(T)+"plita_baseColor.png",
            std::string(T)+"plita_normal.png",
            std::string(T)+"plita_metallicRoughness.png", "");
    // [24] material_24
    MakeMat(std::string(T)+"material_24_baseColor.png",
            std::string(T)+"material_24_normal.png",
            std::string(T)+"material_24_metallicRoughness.png", "");
    // [25] None.006
    MakeMat(std::string(T)+"None.006_baseColor.png",
            std::string(T)+"None.006_normal.png",
            std::string(T)+"None.006_metallicRoughness.png", "");
    // [26] Vitewka
    MakeMat(std::string(T)+"Vitewka_baseColor.png",
            std::string(T)+"Vitewka_normal.png",
            std::string(T)+"Vitewka_metallicRoughness.png", "");
    // [27] Window
    MakeMat(std::string(T)+"Window_baseColor.png",
            std::string(T)+"Window_normal.png",
            std::string(T)+"Window_metallicRoughness.png", "");
    // [28] Windowgl — no textures (glass, fallback white)
    MakeMat("", "", "", "");

    // Submit one RenderItem per submesh, each bound to the correct Material.
    if (m_kitchen.IsValid())
    {
        const Mesh* kitchenMesh = m_kitchen.mesh.get();
        const uint32_t subCount = kitchenMesh->SubMeshCount();

        for (uint32_t si = 0; si < subCount; ++si)
        {
            const SubMesh& sm = kitchenMesh->GetSubMesh(si);
            const uint32_t matIdx = sm.materialIndex;

            if (matIdx >= m_kitchenMats.size())
            {
                spdlog::warn("[NormalMapScene] Kitchen submesh {} has out-of-range matIdx {}", si, matIdx);
                continue;
            }

            auto inst = std::make_unique<MaterialInstance>(m_kitchenMats[matIdx]);
            RenderItem item;
            item.meshMulti    = kitchenMesh;
            item.subMeshIndex = si;
            item.material     = inst.get();
            item.transform.SetScale({2.0f, 2.0f, 2.0f});
            item.transform.SetTranslation({0.0f, 0.0f, 0.0f});
            item.flags.castShadow    = true;
            item.flags.receiveShadow = true;
            AddObject(item);
            m_instances.push_back(std::move(inst));
        }

        spdlog::info("[NormalMapScene] Kitchen: {} submeshes submitted", subCount);
    }

    Camera cam;
    // Camera positioned to see the scene from the side (X-axis view).
    cam.SetPosition({16.0f, 6.0f, 4.0f});
    cam.SetOrientation(180.0f, -20.0f);
    SetCamera(cam);
    SetFirstPersonEyeHeight(1.7f);

    // Neutral sky-grey clear color — kitchen scene is mostly indoors.
    SetClearColor({0.15f, 0.15f, 0.17f, 1.0f});

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

    spdlog::info("[NormalMapScene] Setup complete — bench, avocado, lantern, wooden kitchen ready");
    return true;
}

void NormalMapScene::OnUpdate(float deltaTime, IInputProvider& input)
{
    // Camera anchor at bench height so orbit stays focused on the scene centrepiece.
    glm::vec3 moveDirXZ{0.0f};
    UpdateStandardCameraAndPlayer(deltaTime, input, m_cameraAnchor, moveDirXZ, 0.0f);
}
