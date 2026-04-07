#include "SampleScene.h"

#include "core/Camera.h"
#include "core/KeyboardInput.h"
#include "core/MouseInput.h"
#include "core/Primitives.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

bool SampleScene::Setup() {
    spdlog::info("[SampleScene] Setting up");

    m_shader = std::make_unique<ShaderProgram>("assets/shaders/basic.vert", "assets/shaders/basic.frag");
    if (!m_shader->IsValid()) {
        spdlog::error("[SampleScene] Shader failed to compile/link — aborting setup");
        return false;
    }

    // ── Imported models ───────────────────────────────────────────────────────
    auto meshShader = AssetImporter::LoadShader("assets/shaders/mesh.vert", "assets/shaders/mesh.frag");
    if (!meshShader || !meshShader->IsValid()) {
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

    // ── Geometry ─────────────────────────────────────────────────────────────

    // Rainbow primitives — default ColorMode::Rainbow
    m_triangle   = std::make_unique<MeshBuffer>(GenerateTriangle().CreateMeshBuffer());
    m_quad       = std::make_unique<MeshBuffer>(GenerateQuad().CreateMeshBuffer());
    m_rainbowCube= std::make_unique<MeshBuffer>(GenerateCube().CreateMeshBuffer());
    m_colorQuad  = std::make_unique<MeshBuffer>(
        GenerateQuad({ .colorMode = ColorMode::Solid, 
                       .baseColor = {0.4f, 0.8f, 0.4f} }).CreateMeshBuffer());

    // Solid-coloured cube — steel blue, faces slightly varied so edges read in 3-D
    m_solidCube  = std::make_unique<MeshBuffer>(
        GenerateCube({ .colorMode = ColorMode::Solid,
                       .baseColor = {0.30f, 0.55f, 0.90f},
                       .doubleSided = true }).CreateMeshBuffer());

    // Pyramid — solid warm orange
    m_pyramid    = std::make_unique<MeshBuffer>(
        GeneratePyramid({ .colorMode = ColorMode::Solid,
                          .baseColor = {1.0f, 0.45f, 0.10f} }).CreateMeshBuffer());

    // Sphere — double-sided so wireframe rendering shows all edges (no half-sphere artifact)
    m_sphere     = std::make_unique<MeshBuffer>(
        GenerateSphere(0.4f, 16, { .doubleSided = true }).CreateMeshBuffer());

    // ── Camera ───────────────────────────────────────────────────────────────
    Camera cam;
    // e) a camera starting position that immediately shows depth and perspective
    cam.SetPosition({0.0f, 3.0f, 8.0f});
    cam.SetOrientation(-90.0f, -15.0f); // Look down slightly
    SetCamera(cam);
    SetClearColor({0.08f, 0.09f, 0.12f, 1.0f});

    // ── Objects ───────────────────────────────────────────────────────────────
    
    // d) a ground plane
    RenderItem groundItem;
    groundItem.mesh   = m_colorQuad.get();
    groundItem.shader = m_shader.get();
    groundItem.transform.SetTranslation({0.0f, -1.0f, 0.0f});
    groundItem.transform.SetRotationEulerDegrees({-90.0f, 0.0f, 0.0f});
    groundItem.transform.SetScale({20.0f, 20.0f, 20.0f});
    AddObject(groundItem);

    // a), b), c) several cubes and quads placed at different positions, scales, and rotations
    // Row 1: Cubes
    for (int i = 0; i < 5; ++i) {
        RenderItem cubeItem;
        cubeItem.mesh   = (i % 2 == 0) ? m_rainbowCube.get() : m_solidCube.get();
        cubeItem.shader = m_shader.get();
        
        float xPath = -4.0f + (i * 2.0f);
        cubeItem.transform.SetTranslation({xPath, -0.5f, -2.0f});
        
        float s = 0.5f + (i * 0.15f);
        cubeItem.transform.SetScale({s, s, s});
        
        float rotY = i * 25.0f; 
        float rotX = i * 15.0f;
        cubeItem.transform.SetRotationEulerDegrees({rotX, rotY, 0.0f});
        AddObject(cubeItem);
    }
    
    // Row 2: Quads (visualise transforms)
    for (int i = 0; i < 5; ++i) {
        RenderItem quadItem;
        quadItem.mesh   = m_quad.get();
        quadItem.shader = m_shader.get();
        
        float xPath = -4.0f + (i * 2.0f);
        quadItem.transform.SetTranslation({xPath, 0.0f, -5.0f});
        
        float sx = 0.6f + (i * 0.2f);
        float sy = 0.6f + ((4 - i) * 0.1f);
        quadItem.transform.SetScale({sx, sy, 1.0f});
        
        float rotZ = i * -15.0f; 
        quadItem.transform.SetRotationEulerDegrees({0.0f, 0.0f, rotZ});
        AddObject(quadItem);
    }

    // Row 3: Mixed extra shapes
    RenderItem pyItem;
    pyItem.mesh   = m_pyramid.get();
    pyItem.shader = m_shader.get();
    pyItem.transform.SetTranslation({-2.0f, 1.0f, -8.0f});
    pyItem.transform.SetScale({1.5f, 2.0f, 1.5f});
    m_pyramidIdx = AddObject(pyItem);

    RenderItem sphItem;
    sphItem.mesh     = m_sphere.get();
    sphItem.shader   = m_shader.get();
    sphItem.transform.SetTranslation({2.0f, 1.0f, -8.0f});
    sphItem.transform.SetScale({2.0f, 3.0f, 2.0f});
    AddObject(sphItem);

    // Player Duck
    if (m_duck) {
        RenderItem playerDuck;
        playerDuck.mesh               = m_duck.get();
        playerDuck.material           = m_duckMatInst.get();
        playerDuck.rotationOffsetDeg  = {0.0f, -90.0f, 0.0f};
        playerDuck.translationOffset  = {0.0f, -0.9f, 0.0f};
        playerDuck.transform.SetTranslation(m_playerPosition);
        playerDuck.transform.SetScale({0.6f, 0.6f, 0.6f}); // glTF units are meters; duck is ~0.5 m long
        m_playerCubeIdx = AddObject(playerDuck);
    }

    // ── Lantern (imported glTF) ───────────────────────────────────────────────
    if (m_lantern) {
        RenderItem lanternItem;
        lanternItem.mesh      = m_lantern.get();
        lanternItem.material  = m_lanternMatInst.get();
        lanternItem.transform.SetTranslation({-3.0f, -1.0f, 0.0f});
        lanternItem.transform.SetScale({0.1f, 0.1f, 0.1f}); // raw positions ~25 units tall; scale to ~2.5
        AddObject(lanternItem);
    }

    // ── Avocado (imported glTF) ───────────────────────────────────────────────
    if (m_avocado) {
        RenderItem avocadoItem;
        avocadoItem.mesh      = m_avocado.get();
        avocadoItem.material  = m_avocadoMatInst.get();
        avocadoItem.transform.SetTranslation({0.0f, -1.0f, 0.0f});
        avocadoItem.transform.SetScale({20.0f, 20.0f, 20.0f}); // glTF units are meters; avocado is ~5 cm
        AddObject(avocadoItem);
    }

    spdlog::info("[SampleScene] Ready — detailed test scene with ground, rows of cubes/quads loaded");
    return true;
}

void SampleScene::OnUpdate(float deltaTime, KeyboardInput& input, MouseInput& mouse) {
    // Animate pyramid continuously
    m_pyramidRotY += 60.0f * deltaTime; // 60 degrees per sec
    auto& pyTransform = GetObject(m_pyramidIdx).transform;
    pyTransform.SetRotationEulerDegrees({0.0f, m_pyramidRotY, 0.0f});

    Camera& cam = GetCamera();

    // Hide player mesh in first-person so it doesn't clip into the camera
    if (m_duck)
        GetObject(m_playerCubeIdx).flags.visible = (cam.GetMode() != CameraMode::FirstPerson);

    glm::vec3 moveDirXZ;
    UpdateStandardCameraAndPlayer(deltaTime, input, mouse, m_playerPosition, moveDirXZ, 0.7f);

    // Keep player cube in sync
    auto& playerTransform = GetObject(m_playerCubeIdx).transform;
    playerTransform.SetTranslation(m_playerPosition);

    // Rotate to face the direction of movement (XZ plane only)
    if (glm::length(moveDirXZ) > 0.001f) {
        const glm::vec3 d = glm::normalize(moveDirXZ);
        const float playerYaw = glm::degrees(std::atan2(d.x, d.z));
        playerTransform.SetRotationEulerDegrees({0.0f, playerYaw, 0.0f});
    }
}
