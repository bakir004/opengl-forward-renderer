#pragma once

#include <vector>
#include <glm/glm.hpp>
#include "core/Camera.h"
#include "core/Renderer.h"
#include "RenderItem.h"

struct FrameSubmission {
    const Camera* camera = nullptr;
    Viewport viewport{0, 0, 1280, 720};
    glm::vec4 clearColor{0.1f, 0.1f, 0.1f, 1.0f};
    ClearFlags clearFlags = ClearFlags::ColorDepth;
    std::vector<RenderItem> objects;
    float time = 0.0f;
    float deltaTime = 0.0f;
};
