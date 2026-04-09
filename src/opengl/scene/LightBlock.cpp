#include "scene/LightBlock.h"
#include "scene/LightUtils.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cstring>

LightBlock LightBlock::Pack(const LightEnvironment& env) {
    LightBlock block{};

    block.ambientColor     = env.ambientColor;
    block.ambientIntensity = env.ambientIntensity;

    // ── Directional light ─────────────────────────────────────────────────
    if (env.HasDirectionalLight()) {
        const DirectionalLight& d = env.GetDirectionalLight();
        block.hasDirectional = d.enabled ? 1 : 0;

        auto& gd         = block.directional;
        gd.direction     = glm::normalize(d.direction);
        gd.intensity     = d.intensity;
        gd.color         = d.color;
        gd.enabled       = d.enabled ? 1u : 0u;
        gd.castShadow    = d.shadow.castShadow ? 1u : 0u;
        gd.depthBias     = d.shadow.depthBias;
        gd.normalBias    = d.shadow.normalBias;
        gd._pad0         = 0.0f;
    }

    // ── Point lights ──────────────────────────────────────────────────────
    const auto& points = env.GetPointLights();
    const int numPoints = static_cast<int>(
        std::min(points.size(), static_cast<size_t>(kGpuMaxPointLights)));
    block.numPointLights = numPoints;

    for (int i = 0; i < numPoints; ++i) {
        const PointLight& src = points[i];
        GpuPointLight& dst    = block.pointLights[i];

        dst.position      = src.position;
        dst.intensity     = src.intensity;
        dst.color         = src.color;
        dst.radius        = src.radius;
        dst.attnConstant  = src.attenuation.constant;
        dst.attnLinear    = src.attenuation.linear;
        dst.attnQuadratic = src.attenuation.quadratic;
        dst.enabled       = src.enabled ? 1u : 0u;
    }

    // ── Spot lights ───────────────────────────────────────────────────────
    const auto& spots = env.GetSpotLights();
    const int numSpots = static_cast<int>(
        std::min(spots.size(), static_cast<size_t>(kGpuMaxSpotLights)));
    block.numSpotLights = numSpots;

    for (int i = 0; i < numSpots; ++i) {
        const SpotLight& src = spots[i];
        GpuSpotLight& dst    = block.spotLights[i];

        dst.position      = src.position;
        dst.intensity     = src.intensity;
        dst.direction     = glm::normalize(src.direction);
        dst.radius        = src.radius;
        dst.color         = src.color;
        dst.innerCos      = LightUtils::SpotInnerCosine(src);
        dst.outerCos      = LightUtils::SpotOuterCosine(src);
        dst.attnConstant  = src.attenuation.constant;
        dst.attnLinear    = src.attenuation.linear;
        dst.attnQuadratic = src.attenuation.quadratic;
        dst.enabled       = src.enabled ? 1u : 0u;
        dst._pad0 = dst._pad1 = dst._pad2 = 0.0f;
    }

    return block;
}
