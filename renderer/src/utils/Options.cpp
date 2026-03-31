#include "Options.h"
#include <fstream>
#include <spdlog/spdlog.h>

Options::Options(const std::string& config_path) {
    std::ifstream f(config_path);
    if (!f.is_open()) {
        spdlog::warn("[Options] Config file '{}' not found — using defaults ({}x{}, vsync={})",
            config_path, window.width, window.height, window.vsync);
        return;
    }

    try {
        auto cfg = nlohmann::json::parse(f);

        if (cfg.contains("window"))
            window = cfg.at("window").get<WindowOpts>();

        spdlog::info("[Options] Loaded '{}' (window: {}x{}, vsync={})",
            config_path, window.width, window.height, window.vsync);
    } catch (const std::exception& e) {
        spdlog::error("[Options] Failed to parse '{}': {} — using defaults", config_path, e.what());
    }
}
