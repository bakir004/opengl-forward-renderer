#include "app/Application.h"
#include <spdlog/spdlog.h>
#include <cstddef>
#include <vector>

#include "BistroScene.h"
#include "CapturePresetScene.h"
#include "TerrainScene.h"

int main()
{
    spdlog::info("[TestApp] Starting");

    Application app;
    if (!app.Initialize())
    {
        spdlog::error("[TestApp] Application::Initialize() failed — aborting");
        return -1;
    }

    TerrainScene terrainScene;
    BistroScene bistroScene;
    CapturePresetScene captureBaseline(CapturePresetKind::Baseline);
    CapturePresetScene captureDenseGrid(CapturePresetKind::DenseGrid);
    CapturePresetScene captureMaterialSweep(CapturePresetKind::MaterialSweep);
    CapturePresetScene captureCulling(CapturePresetKind::CullingTest);

    bool terrainOk = terrainScene.Setup();
    bool bistroOk = bistroScene.Setup();
    bool captureBaselineOk = captureBaseline.Setup();
    bool captureDenseGridOk = captureDenseGrid.Setup();
    bool captureMaterialSweepOk = captureMaterialSweep.Setup();
    bool captureCullingOk = captureCulling.Setup();

    if (!terrainOk)
        spdlog::error("[TestApp] TerrainScene::Setup() failed");
    if (!bistroOk)
        spdlog::error("[TestApp] BistroScene::Setup() failed");
    if (!captureBaselineOk)
        spdlog::error("[TestApp] Capture baseline setup failed");
    if (!captureDenseGridOk)
        spdlog::error("[TestApp] Capture dense grid setup failed");
    if (!captureMaterialSweepOk)
        spdlog::error("[TestApp] Capture material sweep setup failed");
    if (!captureCullingOk)
        spdlog::error("[TestApp] Capture culling setup failed");

    std::vector<Scene*> scenes;
    if (terrainOk) scenes.push_back(&terrainScene);
    if (bistroOk) scenes.push_back(&bistroScene);
    if (captureBaselineOk) scenes.push_back(&captureBaseline);
    if (captureDenseGridOk) scenes.push_back(&captureDenseGrid);
    if (captureMaterialSweepOk) scenes.push_back(&captureMaterialSweep);
    if (captureCullingOk) scenes.push_back(&captureCulling);

    for (std::size_t i = 0; i < scenes.size(); ++i)
        spdlog::info("[TestApp] Press {} for {}", i + 1, scenes[i]->GetName());

    if (!scenes.empty())
        app.Run(scenes, 0);
    else
        spdlog::error("[TestApp] No scenes were available to run");

    spdlog::info("[TestApp] Shutting down");
    return 0;
}
