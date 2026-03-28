#pragma once
#include <nlohmann/json.hpp>
#include <string>

struct WindowOpts {
    int width = 1280;
    int height = 720;
    std::string title = "Forward Renderer";
    bool vsync = true;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(WindowOpts, width, height, title, vsync)

struct Options {
    WindowOpts window;
    Options(const std::string& config_path);
};
