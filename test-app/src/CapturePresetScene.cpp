#include "CapturePresetScene.h"

#include "assets/AssetImporter.h"
#include "core/Material.h"
#include "core/Mesh.h"
#include "core/MeshData.h"
#include "core/ShaderProgram.h"
#include "scene/LightBuilder.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>
#include <string>

namespace
{
    void AddSubMesh(MeshData& data, const char* name)
    {
        SubMesh subMesh;
        subMesh.name = name;
        subMesh.indexByteOffset = 0;
        subMesh.indexCount = static_cast<uint32_t>(data.indices.size());
        subMesh.baseVertex = 0;
        subMesh.materialIndex = 0;
        subMesh.hasTangents = true;
        data.submeshes.push_back(subMesh);
    }

    MeshData BuildPlane(float halfExtent)
    {
        MeshData data;
        data.name = "CapturePlane";
        data.vertices = {
            {{-halfExtent, 0.0f, -halfExtent}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{ halfExtent, 0.0f, -halfExtent}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{ halfExtent, 0.0f,  halfExtent}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{-halfExtent, 0.0f,  halfExtent}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        };
        data.indices = {0, 1, 2, 0, 2, 3};
        AddSubMesh(data, "Plane");
        return data;
    }

    MeshData BuildCube()
    {
        MeshData data;
        data.name = "CaptureCube";

        auto addFace = [&](glm::vec3 normal,
                           glm::vec4 tangent,
                           glm::vec3 a,
                           glm::vec3 b,
                           glm::vec3 c,
                           glm::vec3 d)
        {
            const uint32_t base = static_cast<uint32_t>(data.vertices.size());
            data.vertices.push_back({a, normal, {0.0f, 0.0f}, tangent});
            data.vertices.push_back({b, normal, {1.0f, 0.0f}, tangent});
            data.vertices.push_back({c, normal, {1.0f, 1.0f}, tangent});
            data.vertices.push_back({d, normal, {0.0f, 1.0f}, tangent});
            data.indices.insert(data.indices.end(), {
                base + 0, base + 1, base + 2,
                base + 0, base + 2, base + 3,
            });
        };

        constexpr float h = 0.5f;
        addFace({ 0.0f,  0.0f,  1.0f}, { 1.0f, 0.0f,  0.0f, 1.0f}, {-h, -h,  h}, { h, -h,  h}, { h,  h,  h}, {-h,  h,  h});
        addFace({ 0.0f,  0.0f, -1.0f}, {-1.0f, 0.0f,  0.0f, 1.0f}, { h, -h, -h}, {-h, -h, -h}, {-h,  h, -h}, { h,  h, -h});
        addFace({ 1.0f,  0.0f,  0.0f}, { 0.0f, 0.0f, -1.0f, 1.0f}, { h, -h,  h}, { h, -h, -h}, { h,  h, -h}, { h,  h,  h});
        addFace({-1.0f,  0.0f,  0.0f}, { 0.0f, 0.0f,  1.0f, 1.0f}, {-h, -h, -h}, {-h, -h,  h}, {-h,  h,  h}, {-h,  h, -h});
        addFace({ 0.0f,  1.0f,  0.0f}, { 1.0f, 0.0f,  0.0f, 1.0f}, {-h,  h,  h}, { h,  h,  h}, { h,  h, -h}, {-h,  h, -h});
        addFace({ 0.0f, -1.0f,  0.0f}, { 1.0f, 0.0f,  0.0f, 1.0f}, {-h, -h, -h}, { h, -h, -h}, { h, -h,  h}, {-h, -h,  h});

        AddSubMesh(data, "Cube");
        return data;
    }

    MeshData BuildSphere(float radius, int stacks, int slices)
    {
        MeshData data;
        data.name = "CaptureSphere";
        stacks = std::max(stacks, 3);
        slices = std::max(slices, 6);

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

                const glm::vec3 normal = glm::normalize(glm::vec3{
                    sinPhi * cosTheta,
                    cosPhi,
                    sinPhi * sinTheta,
                });

                glm::vec3 tangent{-sinTheta, 0.0f, cosTheta};
                if (glm::dot(tangent, tangent) < 1e-6f)
                    tangent = {1.0f, 0.0f, 0.0f};

                data.vertices.push_back({
                    normal * radius,
                    normal,
                    {u, 1.0f - v},
                    glm::vec4(glm::normalize(tangent), 1.0f),
                });
            }
        }

        const int ring = slices + 1;
        for (int stack = 0; stack < stacks; ++stack)
        {
            for (int slice = 0; slice < slices; ++slice)
            {
                const uint32_t i0 = static_cast<uint32_t>(stack * ring + slice);
                const uint32_t i1 = static_cast<uint32_t>((stack + 1) * ring + slice);
                const uint32_t i2 = i0 + 1;
                const uint32_t i3 = i1 + 1;

                data.indices.push_back(i0);
                data.indices.push_back(i2);
                data.indices.push_back(i1);
                data.indices.push_back(i2);
                data.indices.push_back(i3);
                data.indices.push_back(i1);
            }
        }

        AddSubMesh(data, "Sphere");
        return data;
    }

    const char* PresetName(CapturePresetKind kind)
    {
        switch (kind)
        {
        case CapturePresetKind::Baseline: return "Capture: Baseline";
        case CapturePresetKind::DenseGrid: return "Capture: Dense Grid";
        case CapturePresetKind::MaterialSweep: return "Capture: Material Sweep";
        case CapturePresetKind::CullingTest: return "Capture: Culling Test";
        }
        return "Capture Preset";
    }
}

CapturePresetScene::CapturePresetScene(CapturePresetKind kind)
    : m_kind(kind)
{
}

bool CapturePresetScene::Setup()
{
    spdlog::info("[CapturePresetScene] Setting up {}", PresetName(m_kind));
    SetSceneName(PresetName(m_kind));
    SetClearColor({0.055f, 0.065f, 0.075f, 1.0f});
    SetSkyboxVisible(false);

    m_meshShader = AssetImporter::LoadShader("assets/shaders/mesh.vert", "assets/shaders/mesh.frag");
    if (!m_meshShader || !m_meshShader->IsValid())
    {
        spdlog::error("[CapturePresetScene] Mesh shader failed to load");
        return false;
    }

    m_cubeMesh = std::make_shared<Mesh>(BuildCube());
    m_sphereMesh = std::make_shared<Mesh>(BuildSphere(0.5f, 24, 48));
    m_planeMesh = std::make_shared<Mesh>(BuildPlane(1.0f));

    m_baseMaterial = std::make_shared<Material>(m_meshShader);
    m_baseMaterial->SetVec4("u_TintColor", {1.0f, 1.0f, 1.0f, 1.0f});
    m_baseMaterial->SetFloat("u_AoStrength", 1.0f);

    switch (m_kind)
    {
    case CapturePresetKind::Baseline:
        SetupBaselinePreset();
        break;
    case CapturePresetKind::DenseGrid:
        SetupDenseGridPreset();
        break;
    case CapturePresetKind::MaterialSweep:
        SetupMaterialSweepPreset();
        break;
    case CapturePresetKind::CullingTest:
        SetupCullingPreset();
        break;
    }

    return true;
}

void CapturePresetScene::OnUpdate(float deltaTime, IInputProvider& input)
{
    glm::vec3 moveDirXZ{0.0f};
    UpdateStandardCameraAndPlayer(deltaTime, input, m_cameraAnchor, moveDirXZ, 0.0f);
}

MaterialInstance* CapturePresetScene::CreateMaterial(const char* name,
                                                     glm::vec3 albedo,
                                                     float metallic,
                                                     float roughness,
                                                     glm::vec3 emissive,
                                                     float emissiveStrength)
{
    auto material = std::make_unique<MaterialInstance>(m_baseMaterial);
    material->SetName(name);
    material->SetVec3("u_AlbedoColor", albedo);
    material->SetFloat("u_MetallicValue", metallic);
    material->SetFloat("u_RoughnessValue", roughness);
    material->SetVec3("u_EmissiveColor", emissive);
    material->SetFloat("u_EmissiveStrength", emissiveStrength);
    MaterialInstance* raw = material.get();
    m_materials.push_back(std::move(material));
    return raw;
}

void CapturePresetScene::AddMeshItem(const std::shared_ptr<Mesh>& mesh,
                                     const MaterialInstance* material,
                                     glm::vec3 translation,
                                     glm::vec3 scale,
                                     glm::vec3 rotationDegrees,
                                     bool castShadow,
                                     bool receiveShadow)
{
    RenderItem item;
    item.meshMulti = mesh.get();
    item.subMeshIndex = 0;
    item.material = material;
    item.transform.SetTranslation(translation);
    item.transform.SetScale(scale);
    item.transform.SetRotationEulerDegrees(rotationDegrees);
    item.flags.castShadow = castShadow;
    item.flags.receiveShadow = receiveShadow;
    AddObject(item);
}

void CapturePresetScene::AddGroundAndBackdrop(float groundExtent, const MaterialInstance* material)
{
    AddMeshItem(m_planeMesh, material, {0.0f, -0.02f, 0.0f}, {groundExtent, 1.0f, groundExtent}, {}, false, true);
    AddMeshItem(m_planeMesh,
                material,
                {0.0f, groundExtent * 0.32f, -groundExtent * 0.52f},
                {groundExtent, 1.0f, groundExtent * 0.36f},
                {90.0f, 0.0f, 0.0f},
                false,
                true);
}

void CapturePresetScene::ConfigureSharedLighting(bool shadowsEnabled)
{
    SetAmbientLight({0.045f, 0.052f, 0.060f}, 0.65f);

    auto& lights = GetLights();
    lights.Clear();
    lights.SetDirectionalLight(
        DirectionalLightBuilder()
            .Direction({-0.45f, -1.0f, -0.35f})
            .Color({1.0f, 0.96f, 0.88f})
            .Intensity(3.8f)
            .CastShadow(shadowsEnabled)
            .ShadowResolution(2048, 2048)
            .ShadowBias(0.0045f, 0.025f)
            .PCFRadius(1)
            .Name("CaptureSun")
            .Build());

    lights.AddPointLight(
        PointLightBuilder()
            .Position({-4.4f, 3.4f, 4.2f})
            .Color({0.55f, 0.70f, 1.0f})
            .Intensity(2.0f)
            .Radius(14.0f)
            .Name("CoolFill")
            .Build());
}

void CapturePresetScene::SetupBaselinePreset()
{
    ConfigureSharedLighting(true);

    Camera cam;
    cam.SetPosition({0.0f, 3.0f, 8.2f});
    cam.SetOrientation(-90.0f, -16.0f);
    cam.SetFOV(55.0f);
    SetCamera(cam);
    m_cameraAnchor = {0.0f, 1.4f, 0.0f};

    auto* floorMat = CreateMaterial("mat_floor_neutral", {0.33f, 0.35f, 0.37f}, 0.0f, 0.88f);
    auto* blueMat = CreateMaterial("mat_blue_cube", {0.18f, 0.38f, 0.78f}, 0.0f, 0.46f);
    auto* brassMat = CreateMaterial("mat_brass_sphere", {0.95f, 0.62f, 0.30f}, 0.85f, 0.24f);
    auto* clayMat = CreateMaterial("mat_clay_block", {0.72f, 0.30f, 0.22f}, 0.0f, 0.64f);

    AddGroundAndBackdrop(9.0f, floorMat);
    AddMeshItem(m_cubeMesh, blueMat, {-1.65f, 0.55f, -0.25f}, {1.1f, 1.1f, 1.1f});
    AddMeshItem(m_sphereMesh, brassMat, {0.25f, 0.65f, -0.55f}, {1.3f, 1.3f, 1.3f});
    AddMeshItem(m_cubeMesh, clayMat, {1.85f, 0.35f, 0.35f}, {0.9f, 0.7f, 0.9f}, {0.0f, 32.0f, 0.0f});
}

void CapturePresetScene::SetupDenseGridPreset()
{
    ConfigureSharedLighting(true);

    Camera cam;
    cam.SetPosition({0.0f, 8.6f, 24.0f});
    cam.SetOrientation(-90.0f, -20.0f);
    cam.SetFOV(58.0f);
    SetCamera(cam);
    m_cameraAnchor = {0.0f, 1.4f, -8.0f};

    auto* floorMat = CreateMaterial("mat_grid_floor", {0.23f, 0.25f, 0.27f}, 0.0f, 0.92f);
    std::vector<MaterialInstance*> mats = {
        CreateMaterial("mat_grid_blue", {0.18f, 0.34f, 0.72f}, 0.0f, 0.42f),
        CreateMaterial("mat_grid_green", {0.18f, 0.55f, 0.38f}, 0.0f, 0.58f),
        CreateMaterial("mat_grid_gold", {0.96f, 0.68f, 0.28f}, 0.75f, 0.30f),
        CreateMaterial("mat_grid_dark", {0.26f, 0.27f, 0.31f}, 0.15f, 0.72f),
    };

    AddGroundAndBackdrop(34.0f, floorMat);

    constexpr int columns = 12;
    constexpr int rows = 12;
    constexpr float spacing = 2.25f;
    for (int z = 0; z < rows; ++z)
    {
        for (int x = 0; x < columns; ++x)
        {
            const float fx = (static_cast<float>(x) - (columns - 1) * 0.5f) * spacing;
            const float fz = -static_cast<float>(z) * spacing - 2.0f;
            const float height = 0.65f + 0.08f * static_cast<float>((x + z) % 5);
            const bool sphere = ((x + z) % 4) == 0;
            const auto& mesh = sphere ? m_sphereMesh : m_cubeMesh;
            AddMeshItem(mesh,
                        mats[static_cast<size_t>((x + z) % mats.size())],
                        {fx, height * 0.5f, fz},
                        {0.82f, height, 0.82f},
                        {0.0f, static_cast<float>((x * 13 + z * 7) % 90), 0.0f});
        }
    }
}

void CapturePresetScene::SetupMaterialSweepPreset()
{
    ConfigureSharedLighting(true);

    Camera cam;
    cam.SetPosition({0.0f, 3.2f, 10.8f});
    cam.SetOrientation(-90.0f, -12.0f);
    cam.SetFOV(50.0f);
    SetCamera(cam);
    m_cameraAnchor = {0.0f, 1.4f, 0.0f};

    auto* floorMat = CreateMaterial("mat_sweep_floor", {0.20f, 0.21f, 0.23f}, 0.0f, 0.9f);
    AddGroundAndBackdrop(12.0f, floorMat);

    constexpr int columns = 5;
    constexpr int rows = 4;
    constexpr float xSpacing = 1.75f;
    constexpr float zSpacing = 1.55f;
    for (int row = 0; row < rows; ++row)
    {
        const float metallic = static_cast<float>(row) / static_cast<float>(rows - 1);
        for (int col = 0; col < columns; ++col)
        {
            const float roughness = 0.12f + 0.82f * static_cast<float>(col) / static_cast<float>(columns - 1);
            const glm::vec3 albedo = glm::mix(glm::vec3(0.40f, 0.48f, 0.82f),
                                              glm::vec3(0.95f, 0.68f, 0.30f),
                                              metallic);
            const std::string name = "mat_sweep_r" + std::to_string(row) + "_c" + std::to_string(col);
            auto* mat = CreateMaterial(name.c_str(), albedo, metallic, roughness);

            AddMeshItem(m_sphereMesh,
                        mat,
                        {(static_cast<float>(col) - 2.0f) * xSpacing,
                         0.62f,
                         -static_cast<float>(row) * zSpacing - 0.6f},
                        {1.24f, 1.24f, 1.24f});
        }
    }
}

void CapturePresetScene::SetupCullingPreset()
{
    ConfigureSharedLighting(false);

    Camera cam;
    cam.SetPosition({0.0f, 2.8f, 9.5f});
    cam.SetOrientation(-90.0f, -8.0f);
    cam.SetFOV(46.0f);
    SetCamera(cam);
    m_cameraAnchor = {0.0f, 1.4f, 0.0f};

    auto* floorMat = CreateMaterial("mat_culling_floor", {0.24f, 0.25f, 0.27f}, 0.0f, 0.95f);
    auto* visibleMat = CreateMaterial("mat_culling_visible", {0.18f, 0.54f, 0.82f}, 0.0f, 0.42f);
    auto* sideMat = CreateMaterial("mat_culling_side", {0.78f, 0.34f, 0.18f}, 0.0f, 0.58f);
    auto* rearMat = CreateMaterial("mat_culling_rear", {0.55f, 0.38f, 0.78f}, 0.0f, 0.64f);

    AddGroundAndBackdrop(26.0f, floorMat);

    for (int z = 0; z < 8; ++z)
    {
        for (int x = -1; x <= 1; ++x)
        {
            AddMeshItem(m_cubeMesh,
                        visibleMat,
                        {static_cast<float>(x) * 1.6f, 0.45f, -2.0f - static_cast<float>(z) * 2.0f},
                        {0.9f, 0.9f, 0.9f});
        }
    }

    for (int i = 0; i < 18; ++i)
    {
        const float z = -2.0f - static_cast<float>(i % 9) * 2.2f;
        const float y = 0.45f;
        AddMeshItem(m_sphereMesh, sideMat, {-22.0f, y, z}, {0.95f, 0.95f, 0.95f});
        AddMeshItem(m_sphereMesh, sideMat, { 22.0f, y, z}, {0.95f, 0.95f, 0.95f});
    }

    for (int i = 0; i < 18; ++i)
    {
        const float x = (static_cast<float>(i % 9) - 4.0f) * 2.2f;
        const float z = 15.0f + static_cast<float>(i / 9) * 2.0f;
        AddMeshItem(m_cubeMesh, rearMat, {x, 0.45f, z}, {0.9f, 0.9f, 0.9f});
    }
}
