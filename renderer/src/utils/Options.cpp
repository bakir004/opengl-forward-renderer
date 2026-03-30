#include "Options.h"
#include <fstream>
#include <spdlog/spdlog.h>

Options::Options(const std::string& config_path) {
    std::ifstream f((std::string(config_path)));
    if (!f.is_open()) return;

    try {
        auto cfg = nlohmann::json::parse(f);

        if (cfg.contains("window"))
            window = cfg.at("window").get<WindowOpts>();
    } catch (const std::exception& e) {
        spdlog::error("JSON Error: {}. Check that required properties are present in the settings.json and that the JSON shape is valid", e.what());
    }
}
