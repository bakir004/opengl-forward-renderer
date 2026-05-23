#include "IblValidationScene.h"

#include "assets/AssetImporter.h"
#include "core/Material.h"
#include "core/Mesh.h"
#include "core/MeshData.h"
#include "core/ShaderProgram.h"
#include "core/Skybox.h"
#include "scene/LightBuilder.h"
#include "core/MeshBuffer.h"
#include "core/Texture2D.h"
#include "assets/ModelData.h"
#include "scene/ReflectionProbe.h"
#include <imgui.h>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>

namespace
{

    MeshData BuildUvSphere(float radius, int stacks, int slices)
    {
        MeshData data;
        data.name = "PbrValidationSphere";

        stacks = std::max(stacks, 3);
        slices = std::max(slices, 6);

        data.vertices.reserve(static_cast<size_t>(stacks + 1) * static_cast<size_t>(slices + 1));
        for (int stack = 0; stack <= stacks; ++stack)
        {
            const float v = static_cast<float>(stack) / static_cast<float>(stacks);
            const float phi = v * glm::pi<float>();

            for (int slice = 0; slice <= slices; ++slice)
            {
                const float u = static_cast<float>(slice) / static_cast<float>(slices);
                const float theta = u * glm::two_pi<float>();

                const float sinPhi = std::sin(phi);
                const float cosPhi = std::cos(phi);
                const float sinTheta = std::sin(theta);
                const float cosTheta = std::cos(theta);

                const glm::vec3 normal{
                    sinPhi * cosTheta,
                    cosPhi,
                    sinPhi * sinTheta};

                VertexPNT vertex;
                vertex.position = normal * radius;
                vertex.normal = glm::normalize(normal);
                vertex.uv = {u, 1.0f - v};

                glm::vec3 tangent{-sinTheta, 0.0f, cosTheta};
                if (glm::dot(tangent, tangent) < 1e-6f)
                    tangent = {1.0f, 0.0f, 0.0f};

                vertex.tangent = glm::vec4(glm::normalize(tangent), 1.0f);
                data.vertices.push_back(vertex);
            }
        }

        data.indices.reserve(static_cast<size_t>(stacks) * static_cast<size_t>(slices) * 6);
        const int ring = slices + 1;
        for (int stack = 0; stack < stacks; ++stack)
        {
            for (int slice = 0; slice < slices; ++slice)
            {
                const uint32_t i0 = static_cast<uint32_t>(stack * ring + slice);
                const uint32_t i1 = static_cast<uint32_t>((stack + 1) * ring + slice);
                const uint32_t i2 = static_cast<uint32_t>(i0 + 1);
                const uint32_t i3 = static_cast<uint32_t>(i1 + 1);

                data.indices.push_back(i0);
                data.indices.push_back(i2);
                data.indices.push_back(i1);

                data.indices.push_back(i2);
                data.indices.push_back(i3);
                data.indices.push_back(i1);
            }
        }

        SubMesh subMesh;
        subMesh.name = "Sphere";
        subMesh.indexByteOffset = 0;
        subMesh.indexCount = static_cast<uint32_t>(data.indices.size());
        subMesh.baseVertex = 0;
        subMesh.materialIndex = 0;
        subMesh.hasTangents = true;
        data.submeshes.push_back(subMesh);
        return data;
    }

    MeshData BuildPlane(float halfExtent)
    {
        MeshData data;
        data.name = "PbrValidationPlane";
        data.vertices = {
            {{-halfExtent, 0.0f, -halfExtent}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{halfExtent, 0.0f, -halfExtent}, {0.0f, 1.0f, 0.0f}, {4.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{halfExtent, 0.0f, halfExtent}, {0.0f, 1.0f, 0.0f}, {4.0f, 4.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{-halfExtent, 0.0f, halfExtent}, {0.0f, 1.0f, 0.0f}, {0.0f, 4.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        };
        data.indices = {0, 1, 2, 0, 2, 3};

        SubMesh subMesh;
        subMesh.name = "Plane";
        subMesh.indexByteOffset = 0;
        subMesh.indexCount = static_cast<uint32_t>(data.indices.size());
        subMesh.baseVertex = 0;
        subMesh.materialIndex = 0;
        subMesh.hasTangents = true;
        data.submeshes.push_back(subMesh);
        return data;
    }

    std::unique_ptr<MaterialInstance> MakeSphereMaterial(const std::shared_ptr<Material> &parent,
                                                         glm::vec3 albedo,
                                                         float metallic,
                                                         float roughness)
    {
        auto material = std::make_unique<MaterialInstance>(parent);
        material->SetVec3("u_AlbedoColor", albedo);
        material->SetFloat("u_MetallicValue", metallic);
        material->SetFloat("u_RoughnessValue", roughness);
        material->SetVec4("u_TintColor", {1.0f, 1.0f, 1.0f, 1.0f});
        return material;
    }

} // namespace

bool IblValidationScene::Setup()
{
    spdlog::info("[IblValidationScene] Setting up IBL validation scene");
    SetSceneName("IBL Validation");

    auto meshShader = AssetImporter::LoadShader("assets/shaders/mesh.vert", "assets/shaders/mesh.frag");
    if (!meshShader || !meshShader->IsValid())
    {
        spdlog::error("[IblValidationScene] Mesh shader failed to load");
        return false;
    }

    m_sphereMesh = std::make_shared<Mesh>(BuildUvSphere(0.55f, 48, 96));
    m_planeMesh = std::make_shared<Mesh>(BuildPlane(18.0f));

    m_gridBaseMaterial = std::make_shared<Material>(meshShader);
    m_gridBaseMaterial->SetVec3("u_EmissiveColor", {0.0f, 0.0f, 0.0f});
    m_gridBaseMaterial->SetVec4("u_TintColor", {1.0f, 1.0f, 1.0f, 1.0f});

    m_planeMaterial = std::make_shared<Material>(meshShader);
    m_planeMaterial->SetVec3("u_AlbedoColor", {0.19f, 0.20f, 0.22f});
    m_planeMaterial->SetFloat("u_MetallicValue", 0.0f);
    m_planeMaterial->SetFloat("u_RoughnessValue", 1.0f);
    m_planeMaterial->SetVec4("u_TintColor", {1.0f, 1.0f, 1.0f, 1.0f});

    Camera cam;
    cam.SetPosition({0.0f, 3.4f, 11.6f});
    cam.SetOrientation(-90.0f, -13.5f);
    SetCamera(cam);
    SetFirstPersonEyeHeight(1.7f);
    SetClearColor({0.025f, 0.025f, 0.025f, 1.0f});

    // ── IBL Setup ────────────────────────────────────────────────────────────
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

    SetSkybox(m_mountainsSkybox);
    m_skyboxMode = 0;

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

    SetReflectionProbe(m_mountainsProbe);

    SetAmbientLight({0.08f, 0.08f, 0.08f}, 0.1f);
    auto &lights = GetLights();
    lights.GetPointLights().clear();
    lights.GetSpotLights().clear();
    lights.SetDirectionalLight(
        DirectionalLightBuilder()
            .Direction({-0.32f, -0.78f, -0.54f})
            .Color({1.0f, 0.99f, 0.975f})
            .Intensity(4.0f)
            .CastShadow(false)
            .Name("ValidationSun")
            .Build());

    int objectCount = 0;

    // 1. Polished Chrome Sphere (Metal=1, Rough=0)
    auto chromeMaterial = MakeSphereMaterial(m_gridBaseMaterial, {1.0f, 1.0f, 1.0f}, 1.0f, 0.0f);
    RenderItem chromeSphere;
    chromeSphere.meshMulti = m_sphereMesh.get();
    chromeSphere.subMeshIndex = 0;
    chromeSphere.material = chromeMaterial.get();
    chromeSphere.transform.SetTranslation({-4.0f, 1.0f, 0.5f});
    chromeSphere.flags.castShadow = false;
    chromeSphere.flags.receiveShadow = false;
    m_materials.push_back(std::move(chromeMaterial));
    AddObject(chromeSphere);
    objectCount++;

    // 2. Rough Metal Sphere (Metal=1, Rough=0.8)
    auto roughMetalMaterial = MakeSphereMaterial(m_gridBaseMaterial, {1.0f, 1.0f, 1.0f}, 1.0f, 0.8f);
    RenderItem roughMetalSphere;
    roughMetalSphere.meshMulti = m_sphereMesh.get();
    roughMetalSphere.subMeshIndex = 0;
    roughMetalSphere.material = roughMetalMaterial.get();
    roughMetalSphere.transform.SetTranslation({-2.0f, 1.0f, 0.5f});
    roughMetalSphere.flags.castShadow = false;
    roughMetalSphere.flags.receiveShadow = false;
    m_materials.push_back(std::move(roughMetalMaterial));
    AddObject(roughMetalSphere);
    objectCount++;

    // 3. Gold Sphere (Metal=1, Rough=0.2, Albedo=Gold)
    auto goldMaterial = MakeSphereMaterial(m_gridBaseMaterial, {1.0f, 0.766f, 0.336f}, 1.0f, 0.2f);
    RenderItem goldSphere;
    goldSphere.meshMulti = m_sphereMesh.get();
    goldSphere.subMeshIndex = 0;
    goldSphere.material = goldMaterial.get();
    goldSphere.transform.SetTranslation({0.0f, 1.0f, 0.5f});
    goldSphere.flags.castShadow = false;
    goldSphere.flags.receiveShadow = false;
    m_materials.push_back(std::move(goldMaterial));
    AddObject(goldSphere);
    objectCount++;

    // 4. Plastic/Dielectric Sphere (Metal=0, Rough=0.1)
    auto plasticMaterial = MakeSphereMaterial(m_gridBaseMaterial, {1.0f, 0.1f, 0.1f}, 0.0f, 0.1f);
    RenderItem plasticSphere;
    plasticSphere.meshMulti = m_sphereMesh.get();
    plasticSphere.subMeshIndex = 0;
    plasticSphere.material = plasticMaterial.get();
    plasticSphere.transform.SetTranslation({2.0f, 1.0f, 0.5f});
    plasticSphere.flags.castShadow = false;
    plasticSphere.flags.receiveShadow = false;
    m_materials.push_back(std::move(plasticMaterial));
    AddObject(plasticSphere);
    objectCount++;

    // 5. Rough Dielectric Sphere (Metal=0, Rough=0.8)
    auto roughDielectricMaterial = MakeSphereMaterial(m_gridBaseMaterial, {0.1f, 0.1f, 1.0f}, 0.0f, 0.8f);
    RenderItem roughDielectricSphere;
    roughDielectricSphere.meshMulti = m_sphereMesh.get();
    roughDielectricSphere.subMeshIndex = 0;
    roughDielectricSphere.material = roughDielectricMaterial.get();
    roughDielectricSphere.transform.SetTranslation({4.0f, 1.0f, 0.5f});
    roughDielectricSphere.flags.castShadow = false;
    roughDielectricSphere.flags.receiveShadow = false;
    m_materials.push_back(std::move(roughDielectricMaterial));
    AddObject(roughDielectricSphere);
    objectCount++;

    // 6. Normal-mapped object (Bench)
    m_bench = AssetImporter::Import<MeshBuffer>("assets/models/gltf/bench/scene.gltf");
    if (m_bench)
    {
        m_benchMat = std::make_shared<Material>(meshShader);
        m_benchMat->SetTexture(TextureSlot::Albedo,
            AssetImporter::LoadTexture(
                "assets/models/gltf/bench/MesaBanco.Comedor_baseColor.png",
                TextureColorSpace::sRGB));
        m_benchMat->SetTexture(TextureSlot::Normal,
            AssetImporter::LoadTexture(
                "assets/models/gltf/bench/MesaBanco.Comedor_normal.png",
                TextureColorSpace::Linear));
        
        auto benchRoughMetal = AssetImporter::LoadTexture(
            "assets/models/gltf/bench/MesaBanco.Comedor_metallicRoughness.png",
            TextureColorSpace::Linear);
        m_benchMat->SetTexture(TextureSlot::Roughness, benchRoughMetal);
        m_benchMat->SetTexture(TextureSlot::Metallic,  benchRoughMetal);
        m_benchMat->SetVec4("u_TintColor", {1.0f, 1.0f, 1.0f, 1.0f});

        auto inst = std::make_unique<MaterialInstance>(m_benchMat);
        RenderItem item;
        item.mesh     = m_bench.get();
        item.material = inst.get();
        item.transform.SetTranslation({1.0f, -1.275f, 0.5f});
        item.transform.SetScale({2.5f, 2.5f, 5.0f});
        item.transform.SetRotationEulerDegrees({0.0f, 90.0f, 0.0f});
        item.flags.castShadow    = true;
        item.flags.receiveShadow = true;
        AddObject(item);
        m_materials.push_back(std::move(inst));
        objectCount++;
    }
    else
    {
        spdlog::warn("[IblValidationScene] Bench mesh failed to load");
    }

    // 7. FBX Indoor Plant – placed on the bench end, next to the PBR spheres
    m_fbxPlant = AssetImporter::LoadModel(
        "assets/models/fbx/plant/indoor plant_02_fbx/indoor plant_02_+2.fbx");
    if (m_fbxPlant.IsValid())
    {
        m_fbxPlantBase = std::make_shared<Material>(meshShader);
        m_fbxPlantBase->SetVec4("u_TintColor", {1.0f, 1.0f, 1.0f, 1.0f});

        auto colTex = AssetImporter::LoadTexture(
            "assets/models/fbx/plant/textures/indoor plant_2_COL.jpg",
            TextureColorSpace::sRGB);
        auto norTex = AssetImporter::LoadTexture(
            "assets/models/fbx/plant/textures/indoor plant_2_NOR.jpg",
            TextureColorSpace::Linear);
        auto whiteFallback = std::make_shared<Texture2D>(
            Texture2D::CreateFallback(200, 200, 200, 255));

        const uint32_t subCount = m_fbxPlant.mesh->SubMeshCount();
        for (uint32_t i = 0; i < subCount; ++i)
        {
            auto inst = std::make_unique<MaterialInstance>(m_fbxPlantBase);
            inst->SetTexture(TextureSlot::Albedo, colTex ? colTex : whiteFallback);
            if (norTex)
                inst->SetTexture(TextureSlot::Normal, norTex);
            inst->SetVec3("u_AlbedoColor", {1.0f, 1.0f, 1.0f});
            inst->SetFloat("u_MetallicValue", 0.0f);
            inst->SetFloat("u_RoughnessValue", 0.6f);

            const SubMesh& sm = m_fbxPlant.mesh->GetSubMesh(i);
            RenderItem plantItem;
            plantItem.meshMulti    = m_fbxPlant.mesh.get();
            plantItem.subMeshIndex = i;
            plantItem.material = (sm.materialIndex < m_fbxPlant.materials.size())
                ? inst.get() : inst.get();
            plantItem.transform.SetTranslation({6.0f, 0.45f, 0.5f});
            plantItem.transform.SetScale({0.005f, 0.005f, 0.005f});
            plantItem.transform.SetRotationEulerDegrees({0.0f, 0.0f, 0.0f});
            plantItem.flags.castShadow    = true;
            plantItem.flags.receiveShadow = true;
            AddObject(plantItem);
            m_fbxPlantMats.push_back(std::move(inst));
        }
        spdlog::info("[IblValidationScene] FBX plant loaded ({} submeshes)", subCount);
    }
    else
    {
        spdlog::warn("[IblValidationScene] FBX plant failed to load — check path/filename");
    }

    spdlog::info("[IblValidationScene] Added {} validation objects", objectCount);
    return objectCount == 6;
}

void IblValidationScene::OnUpdate(float deltaTime, IInputProvider &input)
{
    glm::vec3 moveDirXZ{0.0f};
    UpdateStandardCameraAndPlayer(deltaTime, input, m_cameraAnchor, moveDirXZ, 0.0f);
}

void IblValidationScene::OnImGuiRender()
{
    // Top right corner
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(displaySize.x - 15.0f, 15.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::Begin("IBL Validation", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);

    ImGui::Text("Lighting Mode");
    if (ImGui::RadioButton("Environment Light Only", m_lightingMode == 0))
    {
        m_lightingMode = 0;
        if (GetLights().HasDirectionalLight())
        {
            GetLights().GetDirectionalLight().intensity = 0.0f;
        }
    }
    if (ImGui::RadioButton("Environment + Directional Light", m_lightingMode == 1))
    {
        m_lightingMode = 1;
        if (GetLights().HasDirectionalLight())
        {
            GetLights().GetDirectionalLight().intensity = m_dirLightIntensity;
        }
    }

    ImGui::Separator();

    ImGui::Text("Skybox Environment");
    if (ImGui::RadioButton("Night Sky (Mountain)", m_skyboxMode == 0))
    {
        m_skyboxMode = 0;
        SetSkybox(m_mountainsSkybox);
        SetReflectionProbe(m_mountainsProbe);
    }
    if (ImGui::RadioButton("Night Sky (Mody)", m_skyboxMode == 1))
    {
        m_skyboxMode = 1;
        SetSkybox(m_moodySkybox);
        SetReflectionProbe(m_moodyProbe);
    }
    if (ImGui::RadioButton("Outdoor Sky (Daytime)", m_skyboxMode == 3))
    {
        m_skyboxMode = 3;
        SetSkybox(m_outdoorSkybox);
        SetReflectionProbe(m_outdoorProbe);
    }
    if (ImGui::RadioButton("Neutral Room", m_skyboxMode == 2))
    {
        m_skyboxMode = 2;
        SetSkybox(m_neutralSkybox);
        SetReflectionProbe(m_neutralProbe);
    }

    ImGui::End();
}
