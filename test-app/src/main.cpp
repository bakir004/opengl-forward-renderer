#include "app/Application.h"
#include "SampleScene.h"
#include <spdlog/spdlog.h>

int main() {
    spdlog::info("[TestApp] Starting");

    Application app;
    if (!app.Initialize()) {
        spdlog::error("[TestApp] Application::Initialize() failed — aborting");
        return -1;
    }

    SampleScene scene;
    if (!scene.Setup())
        spdlog::warn("[TestApp] SampleScene::Setup() failed — no geometry will be rendered");

    spdlog::info("[TestApp] Tab = toggle mouse look | WASD/Space/LCtrl = move | F1/F2/F3 = camera mode");

    app.Run(scene);

    spdlog::info("[TestApp] Shutting down");
    return 0;
}
