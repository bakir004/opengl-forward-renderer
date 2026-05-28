#include "app/Application.h"
#include <spdlog/spdlog.h>

#include "BistroScene.h"

int main()
{
    spdlog::info("[TestApp] Starting");

    Application app;
    if (!app.Initialize())
    {
        spdlog::error("[TestApp] Application::Initialize() failed — aborting");
        return -1;
    }
    BistroScene bistroScene;
    if (!bistroScene.Setup())
        spdlog::error("[TestApp] BistroScene::Setup() failed — aborting");
    else
    {
        spdlog::info("[TestApp] Press 1 for BistroScene");
        app.Run({&bistroScene}, 0);
    }

    spdlog::info("[TestApp] Shutting down");
    return 0;
}
