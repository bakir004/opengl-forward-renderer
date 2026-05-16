#include "PbrValidationScene.h"

#include "assets/AssetImporter.h"
#include "core/Material.h"
#include "core/Mesh.h"
#include "core/MeshData.h"
#include "core/ShaderProgram.h"
#include "core/Skybox.h"
#include "scene/LightBuilder.h"

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
bool PbrValidationScene::Setup()
{
    spdlog::info("[PbrValidationScene] Setting up Sprint 9 IBL validation scene");
    SetSceneName("PBR + IBL Validation");

    auto meshShader = AssetImporter::LoadShader("assets/shaders/mesh.vert", "assets/shaders/mesh.frag");
    if (!meshShader || !meshShader->IsValid())
    {
        spdlog::error("[PbrValidationScene] Mesh shader failed to load");
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
    // Load the Mountains skybox for IBL (6 faces, one for each cube direction).
    std::vector<std::string> skyboxFaces = {
        "assets/skybox/Mountains/px.png", // positive X (right)
        "assets/skybox/Mountains/nx.png", // negative X (left)
        "assets/skybox/Mountains/py.png", // positive Y (up)
        "assets/skybox/Mountains/ny.png", // negative Y (down)
        "assets/skybox/Mountains/pz.png", // positive Z (front)
        "assets/skybox/Mountains/nz.png", // negative Z (back)
    };
    auto skybox = std::make_shared<Skybox>(skyboxFaces);
    skybox->SetExposure(2.0f); // Boost brightness for clearer IBL visualization
    SetSkybox(skybox);

    // The Renderer will automatically create a reflection probe from the skybox
    // and generate irradiance, prefiltered, and BRDF LUT at runtime.
    SetIblIntensity(1.2f);

    // Reduce ambient light when IBL is active; IBL replaces flat ambient.
    SetAmbientLight({0.08f, 0.08f, 0.08f}, 0.1f);
    auto &lights = GetLights();
    lights.GetPointLights().clear();
    lights.GetSpotLights().clear();
    lights.SetDirectionalLight(
        DirectionalLightBuilder()
            .Direction({-0.32f, -0.78f, -0.54f})
            .Color({1.0f, 0.99f, 0.975f})
            .Intensity(4.0f) // Reduced to ~1/3 so IBL is more visible
            .CastShadow(false)
            .Name("ValidationSun")
            .Build());

    auto floorInst = std::make_unique<MaterialInstance>(m_planeMaterial);
    RenderItem floor;
    floor.meshMulti = m_planeMesh.get();
    floor.subMeshIndex = 0;
    floor.material = floorInst.get();
    floor.transform.SetTranslation({0.0f, -1.22f, 0.5f});
    floor.transform.SetScale({0.70f, 1.0f, 0.44f});
    floor.flags.castShadow = false;
    floor.flags.receiveShadow = false;
    m_materials.push_back(std::move(floorInst));
    AddObject(floor);

    auto wallInst = std::make_unique<MaterialInstance>(m_planeMaterial);
    wallInst->SetVec3("u_AlbedoColor", {0.17f, 0.18f, 0.20f});
    RenderItem wall;
    wall.meshMulti = m_planeMesh.get();
    wall.subMeshIndex = 0;
    wall.material = wallInst.get();
    wall.transform.SetTranslation({0.0f, 3.3f, -3.2f});
    wall.transform.SetRotationEulerDegrees({90.0f, 0.0f, 0.0f});
    wall.transform.SetScale({0.62f, 1.0f, 0.30f});
    wall.flags.castShadow = false;
    wall.flags.receiveShadow = false;
    m_materials.push_back(std::move(wallInst));
    AddObject(wall);

    auto podiumInst = std::make_unique<MaterialInstance>(m_planeMaterial);
    podiumInst->SetVec3("u_AlbedoColor", {0.24f, 0.24f, 0.26f});
    RenderItem podium;
    podium.meshMulti = m_planeMesh.get();
    podium.subMeshIndex = 0;
    podium.material = podiumInst.get();
    podium.transform.SetTranslation({0.0f, -1.05f, 0.9f});
    podium.transform.SetScale({0.22f, 1.0f, 0.08f});
    podium.flags.castShadow = false;
    podium.flags.receiveShadow = false;
    m_materials.push_back(std::move(podiumInst));
    AddObject(podium);

    constexpr int gridSize = 5;
    constexpr float spacing = 1.52f;
    constexpr glm::vec3 baseAlbedo(0.5f, 0.5f, 0.5f);
    constexpr float gridCenterZ = 0.15f;
    constexpr float gridCenterY = 1.95f;

    int sphereCount = 0;
    for (int y = 0; y < gridSize; ++y)
    {
        const float metallic = static_cast<float>(y) / static_cast<float>(gridSize - 1);
        for (int x = 0; x < gridSize; ++x)
        {
            const float roughness = static_cast<float>(x) / static_cast<float>(gridSize - 1);

            auto material = MakeSphereMaterial(m_gridBaseMaterial, baseAlbedo, metallic, roughness);

            RenderItem sphere;
            sphere.meshMulti = m_sphereMesh.get();
            sphere.subMeshIndex = 0;
            sphere.material = material.get();
            sphere.transform.SetTranslation({(static_cast<float>(x) - 2.0f) * spacing,
                                             (static_cast<float>(y) - 2.0f) * spacing + gridCenterY,
                                             gridCenterZ});
            sphere.flags.castShadow = false;
            sphere.flags.receiveShadow = false;

            m_materials.push_back(std::move(material));
            AddObject(sphere);
            ++sphereCount;
        }
    }

    auto goldMaterial = MakeSphereMaterial(m_gridBaseMaterial, {1.0f, 0.766f, 0.336f}, 1.0f, 0.1f);
    RenderItem goldSphere;
    goldSphere.meshMulti = m_sphereMesh.get();
    goldSphere.subMeshIndex = 0;
    goldSphere.material = goldMaterial.get();
    goldSphere.transform.SetTranslation({-5.2f, -0.34f, 0.7f});
    goldSphere.transform.SetScale({1.08f, 1.08f, 1.08f});
    goldSphere.flags.castShadow = false;
    goldSphere.flags.receiveShadow = false;
    m_materials.push_back(std::move(goldMaterial));
    AddObject(goldSphere);

    auto redMaterial = MakeSphereMaterial(m_gridBaseMaterial, {1.0f, 0.1f, 0.1f}, 0.0f, 0.5f);
    RenderItem redSphere;
    redSphere.meshMulti = m_sphereMesh.get();
    redSphere.subMeshIndex = 0;
    redSphere.material = redMaterial.get();
    redSphere.transform.SetTranslation({5.2f, -0.34f, 0.7f});
    redSphere.transform.SetScale({1.08f, 1.08f, 1.08f});
    redSphere.flags.castShadow = false;
    redSphere.flags.receiveShadow = false;
    m_materials.push_back(std::move(redMaterial));
    AddObject(redSphere);

    spdlog::info("[PbrValidationScene] Added {} grid spheres plus 2 reference spheres", sphereCount);
    return sphereCount == 25;
}

void PbrValidationScene::OnUpdate(float deltaTime, IInputProvider &input)
{
    glm::vec3 moveDirXZ{0.0f};
    UpdateStandardCameraAndPlayer(deltaTime, input, m_cameraAnchor, moveDirXZ, 0.0f);
}
