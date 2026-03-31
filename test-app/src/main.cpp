#include "app/Application.h"
#include "SampleScene.h"
#include <spdlog/spdlog.h>

int main()
{
    spdlog::info("TestApp starting...");

    Application app;
    if (!app.Initialize())
    {
        spdlog::error("Failed to initialize Application");
        return -1;
    }

    SampleScene scene;
    if (!scene.Setup("shaders/basic.vert", "shaders/basic.frag"))
        spdlog::warn("SampleScene setup failed; scene will not draw");

    spdlog::info("Rendering triangle, quad, and cube. Press ESC or close window to exit.");

    app.Run([&scene](Renderer& renderer, float time) {
        scene.Render(renderer, time);
    });

    spdlog::info("TestApp shutting down...");
    return 0;
}
