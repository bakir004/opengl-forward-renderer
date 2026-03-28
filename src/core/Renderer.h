#pragma once
#include <nlohmann/json_fwd.hpp>

class Renderer {
    public:
        bool Initialize();
        void RenderFrame();
        void Resize(int width, int height);
        void Shutdown();
};
