#include "app/Application.h"
#include <spdlog/spdlog.h>

#include "BistroScene.h"
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


    bool terrainOk = terrainScene.Setup();
    bool bistroOk = bistroScene.Setup();

    if (!terrainOk)
        spdlog::error("[TestApp] TerrainScene::Setup() failed");
    if (!bistroOk)
        spdlog::error("[TestApp] BistroScene::Setup() failed");

    if (terrainOk)
    {
        spdlog::info("[TestApp] Press 1 for TerrainScene");
        app.Run({&terrainScene, &bistroScene}, 0);

    }

    spdlog::info("[TestApp] Shutting down");
    return 0;
}
