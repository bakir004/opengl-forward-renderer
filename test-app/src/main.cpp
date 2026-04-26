#include "app/Application.h"
#include "SampleScene.h"
#include <spdlog/spdlog.h>

#include "SolarSystemScene.h"
#include "DioramaScene.h"
#include "JapanScene.h"
#include "NeonCityScene.h"

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

    DioramaScene dioramaScene;
    if (!dioramaScene.Setup())
        spdlog::warn("[TestApp] DioramaScene::Setup() failed");

    NeonCityScene neonCityScene;
    if (!neonCityScene.Setup())
        spdlog::warn("[TestApp] NeonCityScene::Setup() failed");

    JapanScene japanScene;
    if (!japanScene.Setup())
        spdlog::warn("[TestApp] JapanScene::Setup() failed");

    spdlog::info("[TestApp] Tab = toggle mouse look | WASD/Space/LCtrl = move | F1/F2/F3 = camera mode");
    spdlog::info(
        "[TestApp] Press 1 for SampleScene | Press 2 for SolarSystemScene | Press 3 for DioramaScene | Press 4 for NeonCityScene | Press 5 for JapanScene");

    app.Run({&sampleScene, &solarSystemScene, &dioramaScene, &neonCityScene, &japanScene}, 0);

    spdlog::info("[TestApp] Shutting down");
    return 0;
}