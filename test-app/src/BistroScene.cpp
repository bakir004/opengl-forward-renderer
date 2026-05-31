#include "BistroScene.h"

#include "core/Material.h"
#include "core/Mesh.h"
#include "core/ShaderProgram.h"
#include "core/Skybox.h"
#include "core/Texture2D.h"
#include "scene/LightBuilder.h"
#include "scene/ReflectionProbe.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

namespace
{
    std::string ToLowerCopy(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }

    bool ContainsAny(const std::string& haystack, std::initializer_list<const char*> needles)
    {
        for (const char* needle : needles)
        {
            if (haystack.find(needle) != std::string::npos)
                return true;
        }
        return false;
    }

    bool IsTransparentMaterial(const std::string& name)
    {
        const std::string lower = ToLowerCopy(name);
        return ContainsAny(lower, {"glass", "wine", "water", "ice", "transparent"});
    }

    bool IsEmissiveMaterial(const std::string& name)
    {
        const std::string lower = ToLowerCopy(name);
        return ContainsAny(lower, {"lamp", "light", "emissive", "sign"});
    }

    std::vector<std::unique_ptr<MaterialInstance>> BuildMaterialInstances(
        const ModelData& model,
        const std::shared_ptr<Material>& baseMaterial,
        const std::shared_ptr<Texture2D>& whiteFallback)
    {
        std::vector<std::unique_ptr<MaterialInstance>> materials;
        materials.reserve(model.materials.size());

        for (size_t materialIndex = 0; materialIndex < model.materials.size(); ++materialIndex)
        {
            const ModelMaterialInfo& matInfo = model.materials[materialIndex];

            auto inst = std::make_unique<MaterialInstance>(baseMaterial);
            inst->SetName(matInfo.name.empty() ? ("BistroMaterial_" + std::to_string(materialIndex)) : matInfo.name);
            inst->SetVec4("u_TintColor", {1.0f, 1.0f, 1.0f, 1.0f});
            inst->SetFloat("u_MetallicValue", matInfo.metallicValue);
            inst->SetFloat("u_RoughnessValue", matInfo.roughnessValue);
            inst->SetFloat("u_NormalScale", matInfo.normalScale);
            inst->SetFloat("u_AoStrength", 1.0f);
            inst->SetFloat("u_EmissiveStrength", 1.0f);

            auto albedo = matInfo.diffusePath.empty()
                ? whiteFallback
                : AssetImporter::LoadTexture(matInfo.diffusePath, TextureColorSpace::sRGB);
            inst->SetTexture(TextureSlot::Albedo, albedo ? albedo : whiteFallback);

            if (!matInfo.normalPath.empty())
            {
                auto normal = AssetImporter::LoadTexture(matInfo.normalPath, TextureColorSpace::Linear);
                if (normal)
                {
                    inst->SetTexture(TextureSlot::Normal, normal);
                    inst->SetFloat("u_FlipNormalMapY", 1.0f);
                }
            }

            if (!matInfo.aoPath.empty())
            {
                auto ao = AssetImporter::LoadTexture(matInfo.aoPath, TextureColorSpace::Linear);
                if (ao)
                    inst->SetTexture(TextureSlot::AO, ao);
            }

            if (!matInfo.specularGlossinessPath.empty())
            {
                auto packed = AssetImporter::LoadTexture(matInfo.specularGlossinessPath, TextureColorSpace::Linear);
                if (packed)
                {
                    inst->SetTexture(TextureSlot::SpecularGlossiness, packed);
                    inst->SetBool("u_IsPackedMetalRough", true);
                    // Bistro ORM: R=AO, G=Roughness, B=Metallic.
                    // FBX doesn't export metallic/roughness factors, so defaults (0 and 0.5)
                    // would scale the texture values down. Set both to 1.0 so the texture
                    // drives the values directly (unless a transparent override comes later).
                    inst->SetFloat("u_MetallicValue", 1.0f);
                    inst->SetFloat("u_RoughnessValue", 1.0f);
                }
            }

            if (!matInfo.emissivePath.empty())
            {
                auto emissive = AssetImporter::LoadTexture(matInfo.emissivePath, TextureColorSpace::sRGB);
                if (emissive)
                {
                    inst->SetTexture(TextureSlot::Emissive, emissive);
                    inst->SetVec3("u_EmissiveColor", {1.0f, 1.0f, 1.0f});
                    inst->SetFloat("u_EmissiveStrength", IsEmissiveMaterial(matInfo.name) ? 1000.0f : 120.0f);
                }
            }

            if (IsTransparentMaterial(matInfo.name))
            {
                inst->SetFloat("u_RoughnessValue", 0.04f);
                inst->SetFloat("u_MetallicValue", 0.0f);
                inst->SetFloat("u_AoStrength", 0.0f);
            }

            materials.push_back(std::move(inst));
        }

        return materials;
    }

    std::vector<RenderItem> BuildModelItems(
        const ModelData& model,
        const std::vector<std::unique_ptr<MaterialInstance>>& materials,
        const char* logLabel)
    {
        std::vector<RenderItem> items;
        if (!model.IsValid())
            return items;

        items.reserve(model.mesh->SubMeshCount());

        const uint32_t subMeshCount = model.mesh->SubMeshCount();
        for (uint32_t subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex)
        {
            const SubMesh& subMesh = model.mesh->GetSubMesh(subMeshIndex);
            const ModelMaterialInfo* matInfo = nullptr;
            if (subMesh.materialIndex < model.materials.size())
                matInfo = &model.materials[subMesh.materialIndex];

            const bool transparent = matInfo && IsTransparentMaterial(matInfo->name);
            const MaterialInstance* material = nullptr;
            if (subMesh.materialIndex < materials.size())
                material = materials[subMesh.materialIndex].get();
            else if (!materials.empty())
                material = materials[0].get();

            RenderItem item;
            item.meshMulti = model.mesh.get();
            item.subMeshIndex = subMeshIndex;
            item.material = material;
            item.flags.castShadow = !transparent;
            item.flags.receiveShadow = !transparent;

            items.push_back(item);
        }

        spdlog::info("[BistroScene] Built {} submesh render items from {}", subMeshCount, logLabel);
        return items;
    }
}

bool BistroScene::Setup()
{
    spdlog::info("[BistroScene] Setting up");
    SetSceneName("Bistro");

    auto meshShader = AssetImporter::LoadShader("assets/shaders/mesh.vert", "assets/shaders/mesh.frag");
    if (!meshShader || !meshShader->IsValid())
    {
        spdlog::error("[BistroScene] Mesh shader failed to load");
        return false;
    }

    m_bistroInteriorModel = AssetImporter::LoadModel("assets/models/fbx/bistro_v5_2/BistroInterior_Wine.fbx");
    m_bistroExteriorModel = AssetImporter::LoadModel("assets/models/fbx/bistro_v5_2/BistroExterior.fbx");
    if (!m_bistroInteriorModel.IsValid())
    {
        spdlog::error("[BistroScene] Bistro interior model failed to load");
        return false;
    }
    if (!m_bistroExteriorModel.IsValid())
    {
        spdlog::error("[BistroScene] Bistro exterior model failed to load");
        return false;
    }

    m_bistroBaseMaterial = std::make_shared<Material>(meshShader);
    m_bistroBaseMaterial->SetVec4("u_TintColor", {1.0f, 1.0f, 1.0f, 1.0f});
    m_bistroBaseMaterial->SetFloat("u_EmissiveStrength", 1.0f);
    m_bistroBaseMaterial->SetFloat("u_AoStrength", 1.0f);
    m_bistroBaseMaterial->SetFloat("u_NormalScale", 1.0f);

    auto whiteFallback = std::make_shared<Texture2D>(Texture2D::CreateFallback(220, 220, 220, 255));
    std::vector<std::string> bistroFaces = {
        "assets/skybox/BistroSky/px.png",
        "assets/skybox/BistroSky/nx.png",
        "assets/skybox/BistroSky/py.png",
        "assets/skybox/BistroSky/ny.png",
        "assets/skybox/BistroSky/pz.png",
        "assets/skybox/BistroSky/nz.png",
    };
    m_bistroSkybox = std::make_shared<Skybox>(bistroFaces);
    m_bistroSkybox->SetExposure(0.9f);

    std::vector<std::string> mountainsFaces = {
        "assets/skybox/Mountains/px.png",
        "assets/skybox/Mountains/nx.png",
        "assets/skybox/Mountains/py.png",
        "assets/skybox/Mountains/ny.png",
        "assets/skybox/Mountains/pz.png",
        "assets/skybox/Mountains/nz.png",
    };
    m_mountainsSkybox = std::make_shared<Skybox>(mountainsFaces);
    m_mountainsSkybox->SetExposure(2.0f);

    std::vector<std::string> moodyFaces = {
        "assets/skybox/Moody/vz_moody_right.png",
        "assets/skybox/Moody/vz_moody_left.png",
        "assets/skybox/Moody/vz_moody_up.png",
        "assets/skybox/Moody/vz_moody_down.png",
        "assets/skybox/Moody/vz_moody_front.png",
        "assets/skybox/Moody/vz_moody_back.png",
    };
    m_moodySkybox = std::make_shared<Skybox>(moodyFaces);
    m_moodySkybox->SetExposure(1.5f);

    std::vector<std::string> neutralFaces = {
        "assets/skybox/NeutralRoom/px.png",
        "assets/skybox/NeutralRoom/nx.png",
        "assets/skybox/NeutralRoom/py.png",
        "assets/skybox/NeutralRoom/ny.png",
        "assets/skybox/NeutralRoom/pz.png",
        "assets/skybox/NeutralRoom/nz.png",
    };
    m_neutralSkybox = std::make_shared<Skybox>(neutralFaces);
    m_neutralSkybox->SetExposure(1.5f);

    std::vector<std::string> outdoorFaces = {
        "assets/skybox/OutdoorSky/px.png",
        "assets/skybox/OutdoorSky/nx.png",
        "assets/skybox/OutdoorSky/py.png",
        "assets/skybox/OutdoorSky/ny.png",
        "assets/skybox/OutdoorSky/pz.png",
        "assets/skybox/OutdoorSky/nz.png",
    };
    m_outdoorSkybox = std::make_shared<Skybox>(outdoorFaces);
    m_outdoorSkybox->SetExposure(1.0f);

    SetSkybox(m_bistroSkybox);
    m_skyboxMode = 0;

    m_bistroProbe = std::make_shared<ReflectionProbe>();
    m_bistroProbe->sourceCubemap = m_bistroSkybox->GetTexture();
    m_bistroProbe->intensity = 0.85f;

    m_mountainsProbe = std::make_shared<ReflectionProbe>();
    m_mountainsProbe->sourceCubemap = m_mountainsSkybox->GetTexture();
    m_mountainsProbe->intensity = 1.2f;

    m_moodyProbe = std::make_shared<ReflectionProbe>();
    m_moodyProbe->sourceCubemap = m_moodySkybox->GetTexture();
    m_moodyProbe->intensity = 1.2f;

    m_neutralProbe = std::make_shared<ReflectionProbe>();
    m_neutralProbe->sourceCubemap = m_neutralSkybox->GetTexture();
    m_neutralProbe->intensity = 1.0f;

    m_outdoorProbe = std::make_shared<ReflectionProbe>();
    m_outdoorProbe->sourceCubemap = m_outdoorSkybox->GetTexture();
    m_outdoorProbe->intensity = 1.0f;

    SetReflectionProbe(m_bistroProbe);
    SetIblIntensity(1.4f);

    // Slightly elevated ambient so deeply shadowed areas are readable.
    SetAmbientLight({0.05f, 0.05f, 0.055f}, 0.18f);
    auto& lights = GetLights();
    lights.GetPointLights().clear();
    lights.GetSpotLights().clear();
    lights.SetDirectionalLight(
        DirectionalLightBuilder()
            .Direction({0.117f, -0.940f, 0.321f})
            .Color({1.0f, 0.97f, 0.92f})
            .Intensity(1.0f)
            .CastShadow(true)
            // 2048 gives good quality at far less GPU cost than 4096;
            // without frustum culling all shadow passes are expensive.
            .ShadowResolution(2048, 2048)
            .ShadowBias(0.0025f, 0.03f)
            .Name("BistroSun")
            .Build());

    // Warm interior fill lights. AttenuationCoeffs(1,0,0) = constant physical
    // term, so falloff is purely (1-d/r)^2 — stays bright across the scene and
    // fades cleanly at the radius edge without the harsh drop-off of the default
    // (1, 0.09, 0.032) tutorial coefficients.
    lights.AddPointLight(
        PointLightBuilder()
            .Position({-12.0f, 4.5f, 6.0f})
            .Color({1.0f, 0.84f, 0.62f})
            .Intensity(1.0f)
            .Radius(100.0f)
            .AttenuationCoeffs(1.0f, 0.0f, 0.0f)
            .Name("BistroWarmFillA")
            .Build());
    lights.AddPointLight(
        PointLightBuilder()
            .Position({7.5f, 4.2f, 1.5f})
            .Color({1.0f, 0.76f, 0.52f})
            .Intensity(1.0f)
            .Radius(100.0f)
            .AttenuationCoeffs(1.0f, 0.0f, 0.0f)
            .Name("BistroWarmFillB")
            .Build());
    lights.AddPointLight(
        PointLightBuilder()
            .Position({-2.0f, 5.0f, -8.0f})
            .Color({0.95f, 0.88f, 0.72f})
            .Intensity(1.0f)
            .Radius(100.0f)
            .AttenuationCoeffs(1.0f, 0.0f, 0.0f)
            .Name("BistroCeilingGlow")
            .Build());

    m_interiorMaterials = BuildMaterialInstances(m_bistroInteriorModel, m_bistroBaseMaterial, whiteFallback);
    m_exteriorMaterials = BuildMaterialInstances(m_bistroExteriorModel, m_bistroBaseMaterial, whiteFallback);

    Camera camera;
    camera.SetPosition({0.0f, 2.4f, 14.0f});
    camera.SetOrientation(-90.0f, -10.0f);
    SetCamera(camera);
    SetFirstPersonEyeHeight(1.7f);
    SetClearColor({0.015f, 0.015f, 0.018f, 1.0f});

    for (const RenderItem& item : BuildModelItems(m_bistroInteriorModel, m_interiorMaterials, "interior"))
        AddObject(item);
    for (const RenderItem& item : BuildModelItems(m_bistroExteriorModel, m_exteriorMaterials, "exterior"))
        AddObject(item);

    return true;
}

void BistroScene::OnUpdate(float deltaTime, IInputProvider& input)
{
    glm::vec3 moveDirXZ{0.0f};
    UpdateStandardCameraAndPlayer(deltaTime, input, m_cameraAnchor, moveDirXZ, 0.0f);
}

void BistroScene::OnImGuiRender()
{
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(displaySize.x - 15.0f, 15.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::Begin("Bistro", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);

    ImGui::Text("Skybox Environment");
    if (ImGui::RadioButton("Bistro Sky", m_skyboxMode == 0))
    {
        m_skyboxMode = 0;
        SetSkybox(m_bistroSkybox);
        SetReflectionProbe(m_bistroProbe);
        SetIblIntensity(1.4f);
    }
    if (ImGui::RadioButton("Night Sky (Mountain)", m_skyboxMode == 1))
    {
        m_skyboxMode = 1;
        SetSkybox(m_mountainsSkybox);
        SetReflectionProbe(m_mountainsProbe);
        SetIblIntensity(1.6f);
    }
    if (ImGui::RadioButton("Night Sky (Mody)", m_skyboxMode == 2))
    {
        m_skyboxMode = 2;
        SetSkybox(m_moodySkybox);
        SetReflectionProbe(m_moodyProbe);
        SetIblIntensity(1.6f);
    }
    if (ImGui::RadioButton("Neutral Room", m_skyboxMode == 3))
    {
        m_skyboxMode = 3;
        SetSkybox(m_neutralSkybox);
        SetReflectionProbe(m_neutralProbe);
        SetIblIntensity(1.4f);
    }
    if (ImGui::RadioButton("Outdoor Sky (Daytime)", m_skyboxMode == 4))
    {
        m_skyboxMode = 4;
        SetSkybox(m_outdoorSkybox);
        SetReflectionProbe(m_outdoorProbe);
        SetIblIntensity(1.6f);
    }

    ImGui::End();
}