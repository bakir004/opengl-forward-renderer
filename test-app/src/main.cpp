#include "app/Application.h"
#include "core/Camera.h"
#include "core/KeyboardInput.h"
#include "core/MouseInput.h"
#include "scene/FrameSubmission.h"
#include "SampleScene.h"
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

int main()
{
    spdlog::info("[TestApp] Starting");

    Application app;
    if (!app.Initialize())
    {
        spdlog::error("[TestApp] Application::Initialize() failed — aborting");
        return -1;
    }

    SampleScene scene;
    if (!scene.Setup("assets/shaders/basic.vert", "assets/shaders/basic.frag"))
        spdlog::warn("[TestApp] SampleScene::Setup() failed — no geometry will be rendered this session");

    Camera        camera;
    int width = 0, height = 0;
    app.GetFramebufferSize(width, height);
    camera.OnResize(width, height);

    KeyboardInput& input = *app.GetKeyboardInput();
    MouseInput&    mouse = *app.GetMouseInput();

    constexpr float kMoveSpeed   = 5.0f;   // world-units per second
    constexpr float kSensitivity = 0.1f;   // degrees per pixel

    spdlog::info("[TestApp] Entering main loop");
    spdlog::info("[TestApp] Tab = toggle mouse look | WASD/Space/LCtrl = move");

    float lastTime = 0.0f;
    glm::vec3 playerPosition = glm::vec3(0.4f, 0.0f, -3.0f);

    app.Run([&](FrameSubmission& submission, float time) {
        const float dt = time - lastTime;
        lastTime = time;

        scene.SetPlayerPosition(playerPosition);
        submission.camera = &camera;

        // --- Toggle mouse capture ---
        if (input.IsKeyPressed(GLFW_KEY_TAB))
            mouse.SetCaptured(!mouse.IsCaptured());

        // --- Camera mode switching ---
        if (input.IsKeyPressed(GLFW_KEY_F1)) {
            camera.SetMode(CameraMode::FreeFly);
            spdlog::info("[Camera] mode set to FreeFly");
        }
        if (input.IsKeyPressed(GLFW_KEY_F2)) {
            camera.SetMode(CameraMode::FirstPerson);
            spdlog::info("[Camera] mode set to FirstPerson");
        }
        if (input.IsKeyPressed(GLFW_KEY_F3)) {
            camera.SetMode(CameraMode::ThirdPerson);
            camera.SetOrbitTarget(glm::vec3(0.0f, 0.0f, 0.0f));
            camera.SetOrbitRadius(5.0f);
            spdlog::info("[Camera] mode set to ThirdPerson");
        }

        // --- Mouse look ---
        if (mouse.IsCaptured())
            camera.Rotate(mouse.GetDeltaX() * kSensitivity,
                         -mouse.GetDeltaY() * kSensitivity);

        // --- Keyboard movement input capture ---
        float forwardInput = 0.0f;
        float rightInput = 0.0f;
        float upInput = 0.0f;

        if (input.IsKeyDown(GLFW_KEY_W)) forwardInput += 1.0f;
        if (input.IsKeyDown(GLFW_KEY_S)) forwardInput -= 1.0f;
        if (input.IsKeyDown(GLFW_KEY_D)) rightInput += 1.0f;
        if (input.IsKeyDown(GLFW_KEY_A)) rightInput -= 1.0f;
        if (input.IsKeyDown(GLFW_KEY_SPACE)) upInput += 1.0f;
        if (input.IsKeyDown(GLFW_KEY_LEFT_CONTROL)) upInput -= 1.0f;

        // -------- FreeFly --------
        if (camera.GetMode() == CameraMode::FreeFly) {
            if (forwardInput > 0.0f)  camera.Move(CameraDirection::Forward,  kMoveSpeed, dt);
            if (forwardInput < 0.0f)  camera.Move(CameraDirection::Backward, kMoveSpeed, dt);
            if (rightInput > 0.0f)    camera.Move(CameraDirection::Right,    kMoveSpeed, dt);
            if (rightInput < 0.0f)    camera.Move(CameraDirection::Left,     kMoveSpeed, dt);
            if (upInput > 0.0f)       camera.Move(CameraDirection::Up,       kMoveSpeed, dt);
            if (upInput < 0.0f)       camera.Move(CameraDirection::Down,     kMoveSpeed, dt);

            // playerPosition unchanged in FreeFly.
        }
        // -------- FirstPerson --------
        else if (camera.GetMode() == CameraMode::FirstPerson) {
            const glm::vec3 camForward = glm::normalize(glm::vec3(camera.GetForward().x, 0.0f, camera.GetForward().z));
            glm::vec3 camRight = glm::normalize(glm::vec3(camera.GetRight().x, 0.0f, camera.GetRight().z));

            glm::vec3 delta = glm::vec3(0.0f);
            delta += camForward * (forwardInput * kMoveSpeed * dt);
            delta += camRight * (rightInput * kMoveSpeed * dt);
            delta += glm::vec3(0.0f, upInput * kMoveSpeed * dt, 0.0f);

            playerPosition += delta;
            scene.SetPlayerPosition(playerPosition);

            // Camera sticks to player position (eye height offset)
            const glm::vec3 eyeOffset = glm::vec3(0.0f, 1.7f, 0.0f);
            camera.SetPosition(playerPosition + eyeOffset);
        }
        // -------- ThirdPerson --------
        else if (camera.GetMode() == CameraMode::ThirdPerson) {
            glm::vec3 camForward = glm::normalize(glm::vec3(camera.GetForward().x, 0.0f, camera.GetForward().z));
            glm::vec3 camRight = glm::normalize(glm::vec3(camera.GetRight().x, 0.0f, camera.GetRight().z));

            glm::vec3 delta = glm::vec3(0.0f);
            delta += camForward * (forwardInput * kMoveSpeed * dt);
            delta += camRight * (rightInput * kMoveSpeed * dt);
            delta += glm::vec3(0.0f, upInput * kMoveSpeed * dt, 0.0f);

            playerPosition += delta;
            scene.SetPlayerPosition(playerPosition);
            camera.SetOrbitTarget(playerPosition);
        }

        const bool moving = (forwardInput != 0.0f || rightInput != 0.0f || upInput != 0.0f) ||
            (mouse.IsCaptured() && (mouse.GetDeltaX() != 0.0f || mouse.GetDeltaY() != 0.0f));

        int width = 0, height = 0;
        app.GetFramebufferSize(width, height);
        camera.OnResize(width, height);

        const bool rotating = mouse.IsCaptured() &&
                              (mouse.GetDeltaX() != 0.0f || mouse.GetDeltaY() != 0.0f);
        if (moving || rotating) {
            const glm::vec3 pos = camera.GetPosition();
            spdlog::info("[Camera] pos=({:.2f}, {:.2f}, {:.2f})  yaw={:.1f}°  pitch={:.1f}°",
                         pos.x, pos.y, pos.z, camera.GetYaw(), camera.GetPitch());
        }

        scene.Render(submission, time);
    });

    spdlog::info("[TestApp] Shutting down");
    return 0;
}
