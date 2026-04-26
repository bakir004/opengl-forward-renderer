#include "NeonCityScene.h"
#include "core/Camera.h"
#include "core/IInputProvider.h"
#include "core/ShaderProgram.h"
#include "core/Texture2D.h"
#include "core/SubMesh.h"
#include "scene/LightBuilder.h"
#include <spdlog/spdlog.h>

bool NeonCityScene::Setup()
{
    spdlog::info("[NeonCityScene] Setting up");
    SetSceneName("Neon City");

    auto meshShader = AssetImporter::LoadShader(
        "assets/shaders/mesh.vert",
        "assets/shaders/mesh.frag");
    if (!meshShader || !meshShader->IsValid())
        return false;

    // Load the neon city with full multi-material support.
    m_cityModel = AssetImporter::LoadModel("assets/models/gltf/dragon_gate_inn_with_neon/scene.gltf");
    if (!m_cityModel.IsValid())
    {
        spdlog::warn("[NeonCityScene] City model failed to load — scene will be empty");
    }

    // One shared base Material (shader + default params).
    m_cityBaseMaterial = std::make_shared<Material>(meshShader);
    m_cityBaseMaterial->SetVec4("u_TintColor", {1.0f, 1.0f, 1.0f, 1.0f});
    m_cityBaseMaterial->SetFloat("u_Shininess", 96.0f);
    m_cityBaseMaterial->SetFloat("u_SpecularStrength", 0.6f);

    // One MaterialInstance per unique material, each with its own diffuse texture.
    auto whiteFallback = std::make_shared<Texture2D>(
        Texture2D::CreateFallback(200, 200, 200, 255));

    for (const ModelMaterialInfo& matInfo : m_cityModel.materials)
    {
        auto inst = std::make_unique<MaterialInstance>(m_cityBaseMaterial);

        std::shared_ptr<Texture2D> tex;
        if (!matInfo.diffusePath.empty())
            tex = AssetImporter::LoadTexture(matInfo.diffusePath, TextureColorSpace::sRGB);

        inst->SetTexture(TextureSlot::Albedo, tex ? tex : whiteFallback);
        m_cityMatInstances.push_back(std::move(inst));
    }

    // Camera: start elevated, looking down at street level
    Camera cam;
    cam.SetPosition({0.0f, 4.0f, 12.0f});
    cam.SetOrientation(-90.0f, -12.0f);
    SetCamera(cam);

    // Dark night sky
    SetClearColor({0.01f, 0.01f, 0.04f, 1.0f});

    // Very low ambient — neon lights do all the work
    SetAmbientLight({0.05f, 0.06f, 0.12f}, 0.25f);

    auto& lights = GetLights();

    // Dim moonlight — just enough to reveal silhouettes
    lights.SetDirectionalLight(
        DirectionalLightBuilder()
            .Direction({-0.2f, -1.0f, -0.15f})
            .Color({0.75f, 0.82f, 1.0f})
            .Intensity(0.15f)
            .CastShadow(true)
            .ShadowResolution(2048, 2048)
            .Name("Moon")
            .Build());

    lights.AddPointLight(
        PointLightBuilder()
            .Position({0.785f, 5.575f, 15.114f})
            .Color({1.0f, 1.0f, 1.0f})
            .Intensity(4.4f)
            .Radius(7.6f)
            .Name("CameraLight")
            .Build());

    lights.AddPointLight(
        PointLightBuilder()
            .Position({-2.392f, 5.625f, 9.594f})
            .Color({1.0f, 1.0f, 1.0f})
            .Intensity(3.0f)
            .Radius(20.0f)
            .Name("Light 2")
            .Build());

    lights.AddPointLight(
        PointLightBuilder()
            .Position({1.011f, 5.605f, 8.914f})
            .Color({0.0f, 0.267f, 1.0f})
            .Intensity(3.0f)
            .Radius(20.0f)
            .Name("Light 3")
            .Build());

    lights.AddPointLight(
        PointLightBuilder()
            .Position({-0.349f, 7.709f, 13.593f})
            .Color({0.910f, 0.0f, 1.0f})
            .Intensity(1.25f)
            .Radius(11.6f)
            .Name("Light 4")
            .Build());

    // One RenderItem per submesh, each bound to the right MaterialInstance.
    if (m_cityModel.IsValid())
    {
        const uint32_t subMeshCount = m_cityModel.mesh->SubMeshCount();
        for (uint32_t i = 0; i < subMeshCount; ++i)
        {
            const SubMesh& sm = m_cityModel.mesh->GetSubMesh(i);

            RenderItem item;
            item.meshMulti    = m_cityModel.mesh.get();
            item.subMeshIndex = i;
            item.material     = m_cityMatInstances[sm.materialIndex].get();
            item.transform.SetTranslation({0.0f, 3.0f, 10.0f});
            item.transform.SetScale({1.0f, 1.0f, 1.0f});
            item.flags.castShadow    = true;
            item.flags.receiveShadow = true;
            AddObject(item);
        }

        spdlog::info("[NeonCityScene] Added {} render items ({} unique materials)",
                     subMeshCount, m_cityModel.materials.size());
    }

    spdlog::info("[NeonCityScene] Setup complete");
    return true;
}

void NeonCityScene::OnUpdate(float deltaTime, IInputProvider& input)
{
    glm::vec3 moveDirXZ;
    UpdateStandardCameraAndPlayer(deltaTime, input, m_playerPosition, moveDirXZ);
}
