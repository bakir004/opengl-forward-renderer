#include "app/Application.h"
#include "core/Camera.h"
#include "core/KeyboardInput.h"
#include "core/MouseInput.h"
#include "SampleScene.h"
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
    KeyboardInput& input = *app.GetKeyboardInput();
    MouseInput&    mouse = *app.GetMouseInput();

    constexpr float kMoveSpeed   = 5.0f;   // world-units per second
    constexpr float kSensitivity = 0.1f;   // degrees per pixel

    spdlog::info("[TestApp] Entering main loop");
    spdlog::info("[TestApp] Tab = toggle mouse look | WASD/Space/LCtrl = move");

    float lastTime = 0.0f;

    app.Run([&](Renderer& renderer, float time) {
        const float dt = time - lastTime;
        lastTime = time;

        // --- Toggle mouse capture ---
        if (input.IsKeyPressed(GLFW_KEY_TAB))
            mouse.SetCaptured(!mouse.IsCaptured());

        // --- Mouse look ---
        // GetDeltaY is positive when the mouse moves down (screen space).
        // Negating it makes moving the mouse down pitch the camera down.
        // Camera::Rotate clamps pitch to ±89°, so rotation boundaries are enforced there.
        if (mouse.IsCaptured())
            camera.Rotate(mouse.GetDeltaX() * kSensitivity,
                         -mouse.GetDeltaY() * kSensitivity);

        // --- Keyboard movement ---
        const bool moving =
            input.IsKeyDown(GLFW_KEY_W) || input.IsKeyDown(GLFW_KEY_S) ||
            input.IsKeyDown(GLFW_KEY_A) || input.IsKeyDown(GLFW_KEY_D) ||
            input.IsKeyDown(GLFW_KEY_SPACE) || input.IsKeyDown(GLFW_KEY_LEFT_CONTROL);

        if (input.IsKeyDown(GLFW_KEY_W))            camera.Move(CameraDirection::Forward,  kMoveSpeed, dt);
        if (input.IsKeyDown(GLFW_KEY_S))            camera.Move(CameraDirection::Backward, kMoveSpeed, dt);
        if (input.IsKeyDown(GLFW_KEY_A))            camera.Move(CameraDirection::Left,     kMoveSpeed, dt);
        if (input.IsKeyDown(GLFW_KEY_D))            camera.Move(CameraDirection::Right,    kMoveSpeed, dt);
        if (input.IsKeyDown(GLFW_KEY_SPACE))        camera.Move(CameraDirection::Up,       kMoveSpeed, dt);
        if (input.IsKeyDown(GLFW_KEY_LEFT_CONTROL)) camera.Move(CameraDirection::Down,     kMoveSpeed, dt);

        // --- Log camera state whenever position or orientation changes ---
        const bool rotating = mouse.IsCaptured() &&
                              (mouse.GetDeltaX() != 0.0f || mouse.GetDeltaY() != 0.0f);
        if (moving || rotating) {
            const glm::vec3 pos = camera.GetPosition();
            spdlog::info("[Camera] pos=({:.2f}, {:.2f}, {:.2f})  yaw={:.1f}°  pitch={:.1f}°",
                         pos.x, pos.y, pos.z, camera.GetYaw(), camera.GetPitch());
        }

        scene.Render(renderer, time);
    });

    spdlog::info("[TestApp] Shutting down");
    return 0;
}
