#pragma once
#include <nlohmann/json.hpp>
#include <string>

/// Window configuration loaded from settings.json.
struct WindowOpts {
    int width = 1280;
    int height = 720;
    std::string title = "Forward Renderer";
    bool vsync = true;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(WindowOpts, width, height, title, vsync)

/// Reads and holds all application configuration parsed from a JSON config file.
/// Missing or malformed config files are handled gracefully — defaults are used.
struct Options {
    WindowOpts window;

    /// Parses the JSON file at config_path and populates config fields.
    /// Falls back to defaults silently if the file is missing; logs an error on malformed JSON.
    /// @param config_path Path to the JSON settings file (e.g. "config/settings.json")
    Options(const std::string& config_path);
};
