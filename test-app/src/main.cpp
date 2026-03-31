#include "app/Application.h"
#include "SampleScene.h"
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
    if (!scene.Setup("shaders/basic.vert", "shaders/basic.frag"))
        spdlog::warn("[TestApp] SampleScene::Setup() failed — no geometry will be rendered this session");

    spdlog::info("[TestApp] Entering main loop");

    app.Run([&scene](Renderer& renderer, float time) {
        scene.Render(renderer, time);
    });

    spdlog::info("[TestApp] Shutting down");
    return 0;
}
