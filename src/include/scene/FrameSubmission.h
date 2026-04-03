#pragma once

#include <vector>
#include "core/Camera.h"
#include "core/FrameClearInfo.h"
#include "core/SubmissionContext.h"
#include "scene/RenderItem.h"

/// Data collected by Scene::BuildSubmission and consumed by Renderer::BeginFrame.
struct FrameSubmission {
    const Camera*     camera  = nullptr;
    FrameClearInfo    clearInfo;   ///< viewport + clear colour + clear flags
    SubmissionContext context;     ///< pipeline state for this frame's main pass
    std::vector<RenderItem> objects;
    float time      = 0.0f;
    float deltaTime = 0.0f;
};
