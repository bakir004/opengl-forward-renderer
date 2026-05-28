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

    std::shared_ptr<Skybox> MakeSkybox(const std::vector<std::string>& faces, float exposure)
    {
        auto skybox = std::make_shared<Skybox>(faces);
        skybox->SetExposure(exposure);
        return skybox;
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

void BistroScene::SetActiveSkybox(int skyboxMode)
{
    m_skyboxMode = skyboxMode;

    switch (m_skyboxMode)
    {
    case 1:
        SetSkybox(m_mountainsSkybox);
        break;
    case 2:
        SetSkybox(m_moodySkybox);
        break;
    case 3:
        SetSkybox(m_neutralSkybox);
        break;
    case 4:
        SetSkybox(m_outdoorSkybox);
        break;
    case 0:
    default:
        SetSkybox(m_bistroSkybox);
        break;
    }

    if (m_probe)
    {
        m_probe->sourceCubemap = GetSkybox() ? GetSkybox()->GetTexture() : nullptr;
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
    m_bistroSkybox = MakeSkybox({
        "assets/skybox/BistroSky/px.png",
        "assets/skybox/BistroSky/nx.png",
        "assets/skybox/BistroSky/py.png",
        "assets/skybox/BistroSky/ny.png",
        "assets/skybox/BistroSky/pz.png",
        "assets/skybox/BistroSky/nz.png",
    }, 0.9f);
    m_mountainsSkybox = MakeSkybox({
        "assets/skybox/Mountains/px.png",
        "assets/skybox/Mountains/nx.png",
        "assets/skybox/Mountains/py.png",
        "assets/skybox/Mountains/ny.png",
        "assets/skybox/Mountains/pz.png",
        "assets/skybox/Mountains/nz.png",
    }, 2.0f);
    m_moodySkybox = MakeSkybox({
        "assets/skybox/Moody/vz_moody_right.png",
        "assets/skybox/Moody/vz_moody_left.png",
        "assets/skybox/Moody/vz_moody_up.png",
        "assets/skybox/Moody/vz_moody_down.png",
        "assets/skybox/Moody/vz_moody_front.png",
        "assets/skybox/Moody/vz_moody_back.png",
    }, 1.5f);
    m_neutralSkybox = MakeSkybox({
        "assets/skybox/NeutralRoom/px.png",
        "assets/skybox/NeutralRoom/nx.png",
        "assets/skybox/NeutralRoom/py.png",
        "assets/skybox/NeutralRoom/ny.png",
        "assets/skybox/NeutralRoom/pz.png",
        "assets/skybox/NeutralRoom/nz.png",
    }, 1.5f);
    m_outdoorSkybox = MakeSkybox({
        "assets/skybox/OutdoorSky/px.png",
        "assets/skybox/OutdoorSky/nx.png",
        "assets/skybox/OutdoorSky/py.png",
        "assets/skybox/OutdoorSky/ny.png",
        "assets/skybox/OutdoorSky/pz.png",
        "assets/skybox/OutdoorSky/nz.png",
    }, 1.0f);

    m_probe = std::make_shared<ReflectionProbe>();
    m_probe->intensity = 0.85f;

    SetActiveSkybox(0);
    SetReflectionProbe(m_probe);
    SetIblIntensity(0.85f);

    SetAmbientLight({0.03f, 0.03f, 0.035f}, 0.08f);
    auto& lights = GetLights();
    lights.GetPointLights().clear();
    lights.GetSpotLights().clear();
    lights.SetDirectionalLight(
        DirectionalLightBuilder()
            .Direction({-0.42f, -0.88f, -0.22f})
            .Color({1.0f, 0.97f, 0.92f})
            .Intensity(1.2f)
            .CastShadow(true)
            .ShadowResolution(4096, 4096)
            .ShadowBias(0.0025f, 0.03f)
            .Name("BistroSun")
            .Build());

    lights.AddPointLight(
        PointLightBuilder()
            .Position({-12.0f, 4.5f, 6.0f})
            .Color({1.0f, 0.84f, 0.62f})
            .Intensity(4.5f)
            .Radius(24.0f)
            .Name("BistroWarmFillA")
            .Build());
    lights.AddPointLight(
        PointLightBuilder()
            .Position({7.5f, 4.2f, 1.5f})
            .Color({1.0f, 0.76f, 0.52f})
            .Intensity(3.8f)
            .Radius(22.0f)
            .Name("BistroWarmFillB")
            .Build());
    lights.AddPointLight(
        PointLightBuilder()
            .Position({-2.0f, 5.0f, -8.0f})
            .Color({0.95f, 0.88f, 0.72f})
            .Intensity(3.5f)
            .Radius(18.0f)
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
    if (ImGui::RadioButton("Bistro", m_skyboxMode == 0))
        SetActiveSkybox(0);
    if (ImGui::RadioButton("Night Sky (Mountain)", m_skyboxMode == 1))
        SetActiveSkybox(1);
    if (ImGui::RadioButton("Night Sky (Mody)", m_skyboxMode == 2))
        SetActiveSkybox(2);
    if (ImGui::RadioButton("Neutral Room", m_skyboxMode == 3))
        SetActiveSkybox(3);
    if (ImGui::RadioButton("Outdoor Sky (Daytime)", m_skyboxMode == 4))
        SetActiveSkybox(4);

    ImGui::End();
}