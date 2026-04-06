#include "DioramaScene.h"
#include "core/Camera.h"
#include "core/KeyboardInput.h"
#include "core/MouseInput.h"
#include "core/Primitives.h"
#include "core/Texture2D.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

bool DioramaScene::Setup() {
    spdlog::info("[DioramaScene] Setting up");

    m_shader = std::make_unique<ShaderProgram>("assets/shaders/basic.vert", "assets/shaders/basic.frag");
    if (!m_shader->IsValid()) {
        return false;
    }

    auto meshShader = AssetImporter::LoadShader("assets/shaders/mesh.vert", "assets/shaders/mesh.frag");
    if (!meshShader || !meshShader->IsValid()) {
        return false;
    }

    // Load models
    m_duck = AssetImporter::Import<MeshBuffer>("assets/models/gltf/duck/Duck.gltf");
    m_duckMaterial = std::make_shared<Material>(meshShader);
    m_duckMaterial->SetTexture(TextureSlot::Albedo, AssetImporter::LoadTexture("assets/models/gltf/duck/DuckCM.png", TextureColorSpace::sRGB));
    m_duckMaterial->SetVec4("u_TintColor", {1.0f, 1.0f, 1.0f, 1.0f});
    m_duckMatInst = std::make_unique<MaterialInstance>(m_duckMaterial);

    m_lantern = AssetImporter::Import<MeshBuffer>("assets/models/gltf/lantern/Lantern.gltf");
    m_lanternMaterial = std::make_shared<Material>(meshShader);
    m_lanternMaterial->SetTexture(TextureSlot::Albedo, AssetImporter::LoadTexture("assets/models/gltf/lantern/Lantern_baseColor.png", TextureColorSpace::sRGB));
    m_lanternMaterial->SetVec4("u_TintColor", {1.0f, 1.0f, 1.0f, 1.0f});
    m_lanternMatInst = std::make_unique<MaterialInstance>(m_lanternMaterial);

    m_indoorPlant = AssetImporter::Import<MeshBuffer>("assets/models/gltf/plant/scene.gltf");
    m_indoorPlantMaterial = std::make_shared<Material>(meshShader);
    m_indoorPlantMaterial->SetVec4("u_TintColor", {0.3f, 0.7f, 0.3f, 1.0f});
    m_indoorPlantMatInst = std::make_unique<MaterialInstance>(m_indoorPlantMaterial);

    m_house = AssetImporter::Import<MeshBuffer>(R"(assets/models/obj/house/20960_Front_Gable_House_v1_NEW.obj)");
    m_houseMaterial = std::make_shared<Material>(meshShader);
    m_houseMaterial->SetTexture(TextureSlot::Albedo, AssetImporter::LoadTexture(R"(assets/models/obj/house/20960_Front_Gable_House_texture.jpg)", TextureColorSpace::sRGB));
    m_houseMaterial->SetVec4("u_TintColor", {1.0f, 1.0f, 1.0f, 1.0f});
    m_houseMatInst = std::make_unique<MaterialInstance>(m_houseMaterial);

    m_grass = AssetImporter::Import<MeshBuffer>(R"(assets/models/obj/grass/10450_Rectangular_Grass_Patch_v1_iterations-2.obj)");
    m_grassMaterial = std::make_shared<Material>(meshShader);
    m_grassMaterial->SetTexture(TextureSlot::Albedo, AssetImporter::LoadTexture(R"(assets/models/obj/grass/10450_Rectangular_Grass_Patch_v1_Diffuse.jpg)", TextureColorSpace::sRGB));
    m_grassMaterial->SetVec4("u_TintColor", {1.0f, 1.0f, 1.0f, 1.0f});
    m_grassMatInst = std::make_unique<MaterialInstance>(m_grassMaterial);

    m_pool = AssetImporter::Import<MeshBuffer>("assets/models/gltf/pool/scene.gltf");
    m_poolMaterial = std::make_shared<Material>(meshShader);
    m_poolMaterial->SetTexture(TextureSlot::Albedo, AssetImporter::LoadTexture("assets/models/gltf/pool/Stone_diffuse.png", TextureColorSpace::sRGB));
    m_poolMaterial->SetVec4("u_TintColor", {1.0f, 1.0f, 1.0f, 1.0f});
    m_poolMatInst = std::make_unique<MaterialInstance>(m_poolMaterial);

    m_tree = AssetImporter::Import<MeshBuffer>("assets/models/gltf/tree/scene.gltf");
    m_treeMaterial = std::make_shared<Material>(meshShader);
    m_treeMaterial->SetTexture(TextureSlot::Albedo, AssetImporter::LoadTexture("assets/models/gltf/tree/textures_baseColor.png", TextureColorSpace::sRGB));
    m_treeMaterial->SetVec4("u_TintColor", {1.0f, 1.0f, 1.0f, 1.0f});
    m_treeMatInst = std::make_unique<MaterialInstance>(m_treeMaterial);

    m_bench = AssetImporter::Import<MeshBuffer>("assets/models/gltf/bench/scene.gltf");
    m_benchMaterial = std::make_shared<Material>(meshShader);
    m_benchMaterial->SetTexture(TextureSlot::Albedo, AssetImporter::LoadTexture("assets/models/gltf/bench/MesaBanco.Comedor_baseColor.png", TextureColorSpace::sRGB));
    m_benchMaterial->SetVec4("u_TintColor", {1.0f, 1.0f, 1.0f, 1.0f});
    m_benchMatInst = std::make_unique<MaterialInstance>(m_benchMaterial);

    // Quad geometry for floors and walls
    m_colorQuad = std::make_unique<MeshBuffer>(GenerateQuad({.colorMode = ColorMode::Solid, .baseColor = {0.6f, 0.4f, 0.2f}}).CreateMeshBuffer()); // Wood inside
    m_wallQuad = std::make_unique<MeshBuffer>(GenerateQuad({.colorMode = ColorMode::Solid, .baseColor = {0.8f, 0.8f, 0.8f}}).CreateMeshBuffer()); // Walls

    // Camera
    Camera cam;
    cam.SetPosition({0.0f, 3.0f, 8.0f});
    cam.SetOrientation(-90.0f, -15.0f);
    SetCamera(cam);
    SetClearColor({0.3f, 0.5f, 0.8f, 1.0f}); // Sky blueish

    // Outside Ground (Grass Patch)
    if (m_grass) {
        RenderItem outGround;
        outGround.mesh = m_grass.get();
        outGround.material = m_grassMatInst.get();
        outGround.transform.SetTranslation({0.0f, -1.5f, 0.0f});
        outGround.transform.SetRotationEulerDegrees({-90.0f, 0.0f, 0.0f});
        outGround.transform.SetScale({0.1f, 0.1f, 0.1f});
        AddObject(outGround);
    }

    // House
    if (m_house) {
        RenderItem house;
        house.mesh = m_house.get();
        house.material = m_houseMatInst.get();
        house.transform.SetTranslation({0.0f, -1.15f, -5.5f});
        house.transform.SetRotationEulerDegrees({-90.0f, 0.0f, 0.0f});
        house.transform.SetScale({0.95f, 0.95f, 0.95f});
        AddObject(house);
    }

    // Bench
    if (m_bench) {
        RenderItem bench;
        bench.mesh = m_bench.get();
        bench.material = m_benchMatInst.get();
        bench.transform.SetTranslation({-9.0f, -0.5f, -8.2f});
        bench.transform.SetScale({0.85f, 0.85f, 0.85f}); 
        bench.transform.SetRotationEulerDegrees({0.0f, -90.0f, 0.0f});
        AddObject(bench);
    }

    // Plant inside
    if (m_indoorPlant) {
        RenderItem plant;
        plant.mesh = m_indoorPlant.get();
        plant.material = m_indoorPlantMatInst.get();
        plant.transform.SetTranslation({-9.0f, 0.4f, -8.2f});
        plant.transform.SetScale({0.7f, 0.7f, 0.7f});
        AddObject(plant);
    }

    // Lantern
    if (m_lantern) {
        RenderItem lantern;
        lantern.mesh = m_lantern.get();
        lantern.material = m_lanternMatInst.get();
        lantern.transform.SetTranslation({-11.0f, -0.65f, -4.7f});
        lantern.transform.SetScale({0.2f, 0.2f, 0.2f});
        AddObject(lantern);

    }

    // Trees
    if (m_tree) {

        for (int i = 0; i < 3; ++i) {
            RenderItem treeItem;
            treeItem.mesh = m_tree.get();
            treeItem.material = m_treeMatInst.get();
            treeItem.transform.SetTranslation({-2.0f + i * 7.0f, -0.65f, -10.5f});
            treeItem.transform.SetScale({0.4f, 0.6f, 0.4f});
            treeItem.transform.SetRotationEulerDegrees({0.0f, i * 73.0f + 15.0f, 0.0f}); // random variation
            AddObject(treeItem);
        }

        for (int i = 0; i < 3; ++i) {
            RenderItem treeItem;
            treeItem.mesh = m_tree.get();
            treeItem.material = m_treeMatInst.get();
            treeItem.transform.SetTranslation({12.0f, -0.65f, -3.5f + i * 7.0f});
            treeItem.transform.SetScale({0.4f, 0.6f, 0.4f});
            treeItem.transform.SetRotationEulerDegrees({0.0f, i * -47.0f - 20.0f, 0.0f}); // random variation
            AddObject(treeItem);
        }
    }

    // Pool
    if (m_pool) {
        RenderItem pool;
        pool.mesh = m_pool.get();
        pool.material = m_poolMatInst.get();
        pool.transform.SetTranslation({8.5f, 0.5f, -6.0f});
        pool.transform.SetScale({0.007f, 0.007f, 0.007f});
        AddObject(pool);
    }

    // Ducks
    if (m_duck) {
        RenderItem duckInside;
        duckInside.mesh = m_duck.get();
        duckInside.material = m_duckMatInst.get();
        duckInside.transform.SetTranslation({4.0f, -0.1f, -3.5f}); // Inside pool
        duckInside.transform.SetRotationEulerDegrees({0.0f, -45.0f, 0.0f});
        duckInside.transform.SetScale({0.2f, 0.2f, 0.2f});
        m_duckInsideIdx = AddObject(duckInside);
    }

    // Player Duck
    if (m_duck) {
        RenderItem playerDuck;
        playerDuck.mesh               = m_duck.get();
        playerDuck.material           = m_duckMatInst.get();
        playerDuck.rotationOffsetDeg  = {0.0f, -90.0f, 0.0f};
        playerDuck.translationOffset  = {0.0f, -0.75f, 0.0f};
        playerDuck.transform.SetTranslation(m_playerPosition);
        playerDuck.transform.SetScale({0.6f, 0.6f, 0.6f});
        m_playerCubeIdx = AddObject(playerDuck);
    }

    spdlog::info("[DioramaScene] Setup complete");
    return true;
}

void DioramaScene::OnUpdate(float deltaTime, KeyboardInput& input, MouseInput& mouse) {
    if (input.IsKeyPressed(GLFW_KEY_TAB))
        mouse.SetCaptured(!mouse.IsCaptured());

    // Swimming duck logic
    m_duckInsideAngle += 1.0f * deltaTime;
    const float radius = 0.7f;
    const float cx = 8.5f;
    const float cz = -6.0f;


    float px = cx + radius * std::cos(m_duckInsideAngle);
    float pz = cz + radius * std::sin(m_duckInsideAngle);

    auto& duckTrans = GetObject(m_duckInsideIdx).transform;
    duckTrans.SetTranslation({px, 0.4f, pz});
    float yaw = glm::degrees(-m_duckInsideAngle) - 90.0f;
    duckTrans.SetRotationEulerDegrees({0.0f, yaw, 0.0f});

    Camera& cam = GetCamera();

    // Hide player mesh in first-person so it doesn't clip into the camera
    if (m_duck)
        GetObject(m_playerCubeIdx).flags.visible = (cam.GetMode() != CameraMode::FirstPerson);

    // Camera mode switching
    if (input.IsKeyPressed(GLFW_KEY_F1)) {
        cam.SetMode(CameraMode::FreeFly);
        spdlog::debug("[Camera] mode: FreeFly");
    }
    if (input.IsKeyPressed(GLFW_KEY_F2)) {
        cam.SetMode(CameraMode::FirstPerson);
        spdlog::debug("[Camera] mode: FirstPerson");
    }
    if (input.IsKeyPressed(GLFW_KEY_F3)) {
        cam.SetMode(CameraMode::ThirdPerson);
        cam.SetOrbitTarget(m_playerPosition);
        cam.SetOrbitRadius(5.0f);
        spdlog::debug("[Camera] mode: ThirdPerson");
    }

    // Mouse look
    if (mouse.IsCaptured())
        cam.Rotate(mouse.GetDeltaX() * 0.1f, -mouse.GetDeltaY() * 0.1f);

    // Axis inputs
    constexpr float freeFlySpeed = 4.0f;
    constexpr float kSpeed = 3.0f;
    float fwd = 0.0f, right = 0.0f, up = 0.0f;
    if (input.IsKeyDown(GLFW_KEY_W))            fwd   += 1.0f;
    if (input.IsKeyDown(GLFW_KEY_S))            fwd   -= 1.0f;
    if (input.IsKeyDown(GLFW_KEY_D))            right += 1.0f;
    if (input.IsKeyDown(GLFW_KEY_A))            right -= 1.0f;
    if (input.IsKeyDown(GLFW_KEY_SPACE))        up    += 1.0f;
    if (input.IsKeyDown(GLFW_KEY_LEFT_CONTROL)) up    -= 1.0f;

    glm::vec3 moveDirXZ(0.0f);  // horizontal movement direction this frame

    if (cam.GetMode() == CameraMode::FreeFly) {
        if (fwd   > 0.0f) cam.Move(CameraDirection::Forward,  freeFlySpeed, deltaTime);
        if (fwd   < 0.0f) cam.Move(CameraDirection::Backward, freeFlySpeed, deltaTime);
        if (right > 0.0f) cam.Move(CameraDirection::Right,    freeFlySpeed, deltaTime);
        if (right < 0.0f) cam.Move(CameraDirection::Left,     freeFlySpeed, deltaTime);
        if (up    > 0.0f) cam.Move(CameraDirection::Up,       freeFlySpeed, deltaTime);
        if (up    < 0.0f) cam.Move(CameraDirection::Down,     freeFlySpeed, deltaTime);
    } else if (cam.GetMode() == CameraMode::FirstPerson) {
        const glm::vec3 fwdXZ   = glm::normalize(glm::vec3(cam.GetForward().x, 0.0f, cam.GetForward().z));
        const glm::vec3 rightXZ = glm::normalize(glm::vec3(cam.GetRight().x,   0.0f, cam.GetRight().z));
        moveDirXZ = fwdXZ * fwd + rightXZ * right;
        m_playerPosition += moveDirXZ * (kSpeed * deltaTime);
        cam.SetPosition(m_playerPosition);
    } else if (cam.GetMode() == CameraMode::ThirdPerson) {
        const glm::vec3 fwdXZ   = glm::normalize(glm::vec3(cam.GetForward().x, 0.0f, cam.GetForward().z));
        const glm::vec3 rightXZ = glm::normalize(glm::vec3(cam.GetRight().x,   0.0f, cam.GetRight().z));
        moveDirXZ = fwdXZ * fwd + rightXZ * right;
        m_playerPosition += moveDirXZ * (kSpeed * deltaTime);
        cam.SetOrbitTarget(m_playerPosition + glm::vec3(0.0f, 0.7f, 0.0f)); // orbit target is above player position
    }

    // Keep player cube in sync
    auto& playerTransform = GetObject(m_playerCubeIdx).transform;
    playerTransform.SetTranslation(m_playerPosition);

    // Rotate to face the direction of movement (XZ plane only)
    if (glm::length(moveDirXZ) > 0.001f) {
        const glm::vec3 d = glm::normalize(moveDirXZ);
        const float yaw = glm::degrees(std::atan2(d.x, d.z));
        playerTransform.SetRotationEulerDegrees({0.0f, yaw, 0.0f});
    }

    // Debug log when moving or looking
    const bool moving   = fwd != 0.0f || right != 0.0f || up != 0.0f;
    const bool rotating = mouse.IsCaptured() && (mouse.GetDeltaX() != 0.0f || mouse.GetDeltaY() != 0.0f);
    if (moving || rotating) {
        const glm::vec3 pos = cam.GetPosition();
        spdlog::debug("[Camera] pos=({:.2f}, {:.2f}, {:.2f})  yaw={:.1f}°  pitch={:.1f}°",
                     pos.x, pos.y, pos.z, cam.GetYaw(), cam.GetPitch());
    }
}
