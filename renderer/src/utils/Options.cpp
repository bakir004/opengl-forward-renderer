#include "Options.h"
#include <filesystem>
#include <fstream>
#include <spdlog/spdlog.h>


std::string ResolveConfigPath(const std::string& config_path) {
    namespace fs = std::filesystem;

    const fs::path requested(config_path);
    if (fs::exists(requested)) {
        return requested.string();
    }

    for (fs::path current = fs::current_path();; current = current.parent_path()) {
        const fs::path candidate = current / requested;
        if (fs::exists(candidate)) {
            return candidate.string();
        }

        if (!current.has_parent_path() || current == current.parent_path()) {
            break;
        }
    }

    return config_path;
}



Options::Options(const std::string& config_path) {
    const std::string resolved_path = ResolveConfigPath(config_path);
    std::ifstream f(resolved_path);
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
            resolved_path, window.width, window.height, window.vsync);
    } catch (const std::exception& e) {
        spdlog::error("[Options] Failed to parse '{}': {} — using defaults", resolved_path, e.what());
    }
}
