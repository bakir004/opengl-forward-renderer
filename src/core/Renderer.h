#pragma once
#include <nlohmann/json_fwd.hpp>

class Renderer {
    public:
        bool Initialize(const nlohmann::json& config);
        void RenderFrame();
        void Resize(int width, int height);
        void Shutdown();
};