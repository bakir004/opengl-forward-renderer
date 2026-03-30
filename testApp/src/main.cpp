#include "app/Application.h"
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

    spdlog::info("Rendering triangle, quad, and cube. Press ESC or close window to exit.");

    // Application::Run() internally uses SampleScene to render all 3 primitives
    app.Run();

    spdlog::info("TestApp shutting down...");
    return 0;
}