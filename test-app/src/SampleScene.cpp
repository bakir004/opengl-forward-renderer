#include "SampleScene.h"

#include "core/Camera.h"
#include "core/KeyboardInput.h"
#include "core/MouseInput.h"
#include "core/Primitives.h"
#include "scene/LightBuilder.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

bool SampleScene::Setup()
{
    spdlog::info("[SampleScene] Setting up");

    m_shader = std::make_unique<ShaderProgram>("assets/shaders/basic.vert", "assets/shaders/basic.frag");
    if (!m_shader->IsValid())
    {
        spdlog::error("[SampleScene] Shader failed to compile/link — aborting setup");
        return false;
    }

    // ── Imported models ───────────────────────────────────────────────────────
    auto meshShader = AssetImporter::LoadShader("assets/shaders/mesh.vert", "assets/shaders/mesh.frag");
    if (!meshShader || !meshShader->IsValid())
    {
        spdlog::error("[SampleScene] Mesh shader failed to compile/link — aborting setup");
        return false;
    }

    m_avocado = AssetImporter::Import<MeshBuffer>("assets/models/gltf/avocado/Avocado.gltf");
    m_avocadoMaterial = std::make_shared<Material>(meshShader);
    m_avocadoMaterial->SetTexture(TextureSlot::Albedo,
                                  AssetImporter::LoadTexture("assets/models/gltf/avocado/Avocado_baseColor.png", TextureColorSpace::sRGB));
    m_avocadoMaterial->SetVec4("u_TintColor", {1.0f, 1.0f, 1.0f, 1.0f});
    m_avocadoMatInst = std::make_unique<MaterialInstance>(m_avocadoMaterial);

    m_duck = AssetImporter::Import<MeshBuffer>("assets/models/gltf/duck/Duck.gltf");
    m_duckMaterial = std::make_shared<Material>(meshShader);
    m_duckMaterial->SetTexture(TextureSlot::Albedo,
                               AssetImporter::LoadTexture("assets/models/gltf/duck/DuckCM.png", TextureColorSpace::sRGB));
    m_duckMaterial->SetVec4("u_TintColor", {1.0f, 1.0f, 1.0f, 1.0f});
    m_duckMatInst = std::make_unique<MaterialInstance>(m_duckMaterial);

    m_lantern = AssetImporter::Import<MeshBuffer>("assets/models/gltf/lantern/Lantern.gltf");
    m_lanternMaterial = std::make_shared<Material>(meshShader);
    m_lanternMaterial->SetTexture(TextureSlot::Albedo,
                                  AssetImporter::LoadTexture("assets/models/gltf/lantern/Lantern_baseColor.png", TextureColorSpace::sRGB));
    m_lanternMaterial->SetVec4("u_TintColor", {1.0f, 1.0f, 1.0f, 1.0f});
    m_lanternMatInst = std::make_unique<MaterialInstance>(m_lanternMaterial);

    // --- ADDED --- Ground plane material with mesh shader for shadow receiving
    m_groundMaterial = std::make_shared<Material>(meshShader);
    m_groundMaterial->SetVec4("u_TintColor", {0.65f, 0.65f, 0.65f, 1.0f}); // --- FIXED --- Lighter gray for shadow contrast
    m_groundMatInst = std::make_unique<MaterialInstance>(m_groundMaterial);

    // ── Geometry ─────────────────────────────────────────────────────────────

    // Rainbow primitives — default ColorMode::Rainbow
    m_triangle = std::make_unique<MeshBuffer>(GenerateTriangle().CreateMeshBuffer());
    m_quad = std::make_unique<MeshBuffer>(GenerateQuad().CreateMeshBuffer());
    m_rainbowCube = std::make_unique<MeshBuffer>(GenerateCube().CreateMeshBuffer());
    m_colorQuad = std::make_unique<MeshBuffer>(
        GenerateQuad({.colorMode = ColorMode::Solid,
                      .baseColor = {0.4f, 0.8f, 0.4f}})
            .CreateMeshBuffer());

    // Solid-coloured cube — steel blue, faces slightly varied so edges read in 3-D
    m_solidCube = std::make_unique<MeshBuffer>(
        GenerateCube({.colorMode = ColorMode::Solid,
                      .baseColor = {0.30f, 0.55f, 0.90f},
                      .doubleSided = true})
            .CreateMeshBuffer());

    // Pyramid — solid warm orange
    m_pyramid = std::make_unique<MeshBuffer>(
        GeneratePyramid({.colorMode = ColorMode::Solid,
                         .baseColor = {1.0f, 0.45f, 0.10f}})
            .CreateMeshBuffer());

    // Sphere — double-sided so wireframe rendering shows all edges (no half-sphere artifact)
    m_sphere = std::make_unique<MeshBuffer>(
        GenerateSphere(0.4f, 16, {.doubleSided = true}).CreateMeshBuffer());

    // ── Camera ───────────────────────────────────────────────────────────────
    Camera cam;
    // e) a camera starting position that immediately shows depth and perspective
    cam.SetPosition({0.0f, 3.0f, 8.0f});
    cam.SetOrientation(-90.0f, -15.0f); // Look down slightly
    SetCamera(cam);
    SetClearColor({0.08f, 0.09f, 0.12f, 1.0f});

    // ── Lights (Dev3: scene-side setup) ───────────────────────────────────
    SetAmbientLight({0.02f, 0.03f, 0.04f}, 0.15f); // --- FIXED --- Extreme reduction to show shadows clearly
    auto &lights = GetLights();
    // --- FIXED --- Very strong directional light to make shadows unmissable
    lights.SetDirectionalLight(
        DirectionalLightBuilder()
            .Direction({-0.3f, -0.8f, -0.2f}) // Balanced angle for readable shadows
            .Color({1.0f, 0.98f, 0.95f})
            .Intensity(3.5f) // --- FIXED --- Dramatically increased from 2.5 to make shadows obvious
            .Name("SampleSun")
            .CastShadow(true)
            .Build());
    // --- FIXED --- Drastically reduced point lights to prevent shadow wash-out
    lights.AddPointLight(
        PointLightBuilder()
            .Position({-3.0f, 2.5f, 2.0f})
            .Color({1.0f, 0.75f, 0.50f}) // Warm orange
            .Intensity(0.6f)             // --- FIXED --- Reduced from 3.0 (was washing out shadows)
            .Radius(18.0f)
            .Name("WarmFill")
            .Build());
    lights.AddPointLight(
        PointLightBuilder()
            .Position({3.5f, 2.2f, -2.5f})
            .Color({0.40f, 0.70f, 1.0f}) // Cool blue
            .Intensity(0.5f)             // --- FIXED --- Reduced from 2.5 (was washing out shadows)
            .Radius(18.0f)
            .Name("CoolRim")
            .Build());

    // --- ADDED --- Lantern light: warm glow from inside lantern
    m_lanternLightIdx = lights.GetPointLights().size();
    lights.AddPointLight(
        PointLightBuilder()
            .Position({-2.5f, 0.8f, 2.0f}) // At lantern position
            .Color({1.0f, 0.85f, 0.50f})   // Warm amber glow
            .Intensity(0.8f)               // Moderate intensity
            .Radius(12.0f)
            .Name("LanternLight")
            .Build());

    // ── Objects ───────────────────────────────────────────────────────────────

    // --- MODIFIED --- Ground plane now uses mesh shader+material for shadow receiving
    RenderItem groundItem;
    groundItem.mesh = m_colorQuad.get();
    groundItem.material = m_groundMatInst.get(); // Use material with mesh shader
    groundItem.transform.SetTranslation({0.0f, 0.0f, 0.0f});
    groundItem.transform.SetRotationEulerDegrees({-90.0f, 0.0f, 0.0f});
    groundItem.transform.SetScale({25.0f, 25.0f, 25.0f});
    groundItem.flags.castShadow = false;
    groundItem.flags.receiveShadow = true;
    AddObject(groundItem);

    // --- REMOVED --- Simplified scene: removed primitive cube/quad rows to focus on shadow demo
    // --- FIXED --- Raised objects to make shadows more visible (further from ground)
    // Pyramid (note: uses basic shader, won't show shadows but still casts them)
    RenderItem pyItem;
    pyItem.mesh = m_pyramid.get();
    pyItem.shader = m_shader.get();
    pyItem.transform.SetTranslation({-3.0f, 0.8f, -4.0f}); // --- FIXED --- Raised from 0.5 to 0.8
    pyItem.transform.SetScale({1.8f, 2.2f, 1.8f});
    pyItem.flags.castShadow = true;
    pyItem.flags.receiveShadow = true;
    m_pyramidIdx = AddObject(pyItem);

    // Sphere (note: uses basic shader, won't show shadows but still casts them)
    RenderItem sphItem;
    sphItem.mesh = m_sphere.get();
    sphItem.shader = m_shader.get();
    sphItem.transform.SetTranslation({3.5f, 0.9f, -3.5f}); // --- FIXED --- Raised from 0.6 to 0.9
    sphItem.transform.SetScale({2.2f, 3.2f, 2.2f});
    sphItem.flags.castShadow = true;
    sphItem.flags.receiveShadow = true;
    AddObject(sphItem);

    // --- FIXED --- Player Duck raised above ground to cast visible shadow
    if (m_duck)
    {
        RenderItem playerDuck;
        playerDuck.mesh = m_duck.get();
        playerDuck.material = m_duckMatInst.get();
        playerDuck.rotationOffsetDeg = {0.0f, -90.0f, 0.0f};
        playerDuck.translationOffset = {0.0f, 0.0f, 0.0f};
        playerDuck.transform.SetTranslation({0.0f, 1.1f, 0.0f}); // --- FIXED --- Raised above ground to cast visible shadow
        playerDuck.transform.SetScale({0.8f, 0.8f, 0.8f});
        playerDuck.flags.castShadow = true;
        playerDuck.flags.receiveShadow = true;
        m_playerCubeIdx = AddObject(playerDuck);
    }

    // --- FIXED --- Lantern raised much higher to create visible shadow
    if (m_lantern)
    {
        RenderItem lanternItem;
        lanternItem.mesh = m_lantern.get();
        lanternItem.material = m_lanternMatInst.get();
        lanternItem.transform.SetTranslation({-2.5f, 0.1f, 2.0f}); // --- FIXED --- Raised from 0.1 to 0.8
        lanternItem.transform.SetScale({0.12f, 0.12f, 0.12f});
        lanternItem.flags.castShadow = true;
        lanternItem.flags.receiveShadow = true;
        AddObject(lanternItem);
    }

    // --- FIXED --- Avocado raised much higher to create visible shadow
    if (m_avocado)
    {
        RenderItem avocadoItem;
        avocadoItem.mesh = m_avocado.get();
        avocadoItem.material = m_avocadoMatInst.get();
        avocadoItem.transform.SetTranslation({2.0f, 0.7f, -1.5f}); // --- FIXED --- Raised from 0.3 to 0.7
        avocadoItem.transform.SetScale({24.0f, 24.0f, 24.0f});
        avocadoItem.flags.castShadow = true;
        avocadoItem.flags.receiveShadow = true;
        AddObject(avocadoItem);
    }

    spdlog::info("[SampleScene] Ready — detailed test scene with ground, rows of cubes/quads loaded");
    return true;
}

void SampleScene::OnUpdate(float deltaTime, KeyboardInput &input, MouseInput &mouse)
{
    // --- ADDED --- Track time for light animation
    m_lightAnimTime += deltaTime;
    m_duckFloatTime += deltaTime;

    // Animate pyramid continuously
    m_pyramidRotY += 60.0f * deltaTime; // 60 degrees per sec
    auto &pyTransform = GetObject(m_pyramidIdx).transform;
    pyTransform.SetRotationEulerDegrees({0.0f, m_pyramidRotY, 0.0f});

    Camera &cam = GetCamera();

    // Hide player mesh in first-person so it doesn't clip into the camera
    if (m_duck)
        GetObject(m_playerCubeIdx).flags.visible = (cam.GetMode() != CameraMode::FirstPerson);

    glm::vec3 moveDirXZ;
    UpdateStandardCameraAndPlayer(deltaTime, input, mouse, m_playerPosition, moveDirXZ, 0.7f);

    // Keep player cube in sync
    auto &playerTransform = GetObject(m_playerCubeIdx).transform;
    playerTransform.SetTranslation(m_playerPosition);

    // Rotate to face the direction of movement (XZ plane only)
    if (glm::length(moveDirXZ) > 0.001f)
    {
        const glm::vec3 d = glm::normalize(moveDirXZ);
        const float playerYaw = glm::degrees(std::atan2(d.x, d.z));
        playerTransform.SetRotationEulerDegrees({0.0f, playerYaw, 0.0f});
    }

    // --- ADDED --- Duck floating animation (bobs up and down)
    // Applied AFTER position sync so it doesn't get overwritten
    const float floatAmplitude = 0.3f; // How far up/down it floats
    const float floatPeriod = 4.0f;    // Seconds for one complete cycle
    const float duckFloatOffset = std::sin((m_duckFloatTime / floatPeriod) * glm::two_pi<float>()) * floatAmplitude;

    if (m_duck && m_playerCubeIdx > 0)
    {
        auto &pTransform = GetObject(m_playerCubeIdx).transform;
        const glm::vec3 pos = pTransform.GetTranslation();
        pTransform.SetTranslation({pos.x, 1.1f + duckFloatOffset, pos.z});
    }

    // --- ADDED --- Toggle lantern light with 'L' key
    if (input.IsKeyPressed(GLFW_KEY_L))
    {
        m_lanternLightEnabled = !m_lanternLightEnabled;
        auto &lights = GetLights();
        auto &pointLights = lights.GetPointLights();
        if (m_lanternLightIdx < pointLights.size())
        {
            pointLights[m_lanternLightIdx].intensity = m_lanternLightEnabled ? 0.8f : 0.0f;
        }
    }

    // --- MODIFIED --- Animate "CoolRim" point light for visual validation
    // Light orbits around center objects to demonstrate dynamic shadow changes
    {
        auto &lights = GetLights();
        auto &pointLights = lights.GetPointLights();
        for (auto &light : pointLights)
        {
            if (light.name == "CoolRim")
            {
                // Orbit at smaller radius ~3.5 units around center, height ~2.2, period ~10 seconds
                const float orbitRadius = 3.5f;  // --- MODIFIED --- Reduced from 5.5 to focus on center
                const float orbitHeight = 2.2f;  // --- MODIFIED --- Raised for better shadows
                const float orbitPeriod = 10.0f; // --- MODIFIED --- Slower orbit for easier observation
                const float angle = (m_lightAnimTime / orbitPeriod) * glm::two_pi<float>();
                light.position = glm::vec3(
                    std::cos(angle) * orbitRadius,
                    orbitHeight,
                    std::sin(angle) * orbitRadius);
                break;
            }
        }
    }
}
