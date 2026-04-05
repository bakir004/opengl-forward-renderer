#include "app/Application.h"
#include "SampleScene.h"
#include <spdlog/spdlog.h>

#include "SolarSystemScene.h"

int main() {
    spdlog::info("[TestApp] Starting");

    Application app;
    if (!app.Initialize()) {
        spdlog::error("[TestApp] Application::Initialize() failed — aborting");
        return -1;
    }

    SampleScene sampleScene;
    if (!sampleScene.Setup())
        spdlog::warn("[TestApp] SampleScene::Setup() failed — scene may render incomplete");

    SolarSystemScene solarSystemScene;
    if (!solarSystemScene.Setup())
        spdlog::warn("[TestApp] SolarSystemScene::Setup() failed — no geometry will be rendered");

    spdlog::info("[TestApp] Tab = toggle mouse look | WASD/Space/LCtrl = move | F1/F2/F3 = camera mode");
    spdlog::info("[TestApp] Press 1 for SampleScene | Press 2 for SolarSystemScene");

    app.Run({&sampleScene, &solarSystemScene}, 0);

    spdlog::info("[TestApp] Shutting down");
    return 0;
}
