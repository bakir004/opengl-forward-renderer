#pragma once

#include <vector>
#include "core/Camera.h"
#include "core/FrameClearInfo.h"
#include "core/SubmissionContext.h"
#include "core/Skybox.h"
#include "scene/RenderItem.h"
#include "scene/LightEnvironment.h"
#include "scene/ReflectionProbe.h"

/// Data collected by Scene::BuildSubmission and consumed by Renderer::BeginFrame.
struct FrameSubmission {
    const Camera*     camera  = nullptr;
    const Skybox*     skybox  = nullptr;
    const ReflectionProbe* activeReflectionProbe = nullptr;
    FrameClearInfo    clearInfo;
    SubmissionContext context;
    LightEnvironment  lights;
    std::vector<RenderItem> objects;
    float time      = 0.0f;
    float deltaTime = 0.0f;
};
 