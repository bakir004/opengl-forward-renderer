#include "scene/LightUtils.h"
#include <spdlog/spdlog.h>

namespace LightUtils {

bool Validate(const LightEnvironment& env) {
    bool ok = true;

    if (env.HasDirectionalLight()) {
        const DirectionalLight& d = env.GetDirectionalLight();
        if (glm::length(d.direction) < 0.001f) {
            spdlog::warn("[LightUtils] Directional light '{}' has near-zero direction vector", d.name);
            ok = false;
        }
        if (d.intensity < 0.0f) {
            spdlog::warn("[LightUtils] Directional light '{}' has negative intensity", d.name);
            ok = false;
        }
        if (d.shadow.castShadow && d.shadow.shadowMapWidth <= 0) {
            spdlog::warn("[LightUtils] Directional light '{}' shadow map has invalid resolution", d.name);
            ok = false;
        }
    }

    const auto& points = env.GetPointLights();
    for (const auto& p : points) {
        if (p.radius <= 0.0f) {
            spdlog::warn("[LightUtils] Point light '{}' has non-positive radius", p.name);
            ok = false;
        }
        if (p.intensity < 0.0f) {
            spdlog::warn("[LightUtils] Point light '{}' has negative intensity", p.name);
            ok = false;
        }
    }

    const auto& spots = env.GetSpotLights();
    for (const auto& s : spots) {
        if (s.cone.innerDeg >= s.cone.outerDeg) {
            spdlog::warn("[LightUtils] Spot light '{}' inner cone ({:.1f}) >= outer cone ({:.1f})",
                         s.name, s.cone.innerDeg, s.cone.outerDeg);
            ok = false;
        }
        if (s.cone.outerDeg <= 0.0f || s.cone.outerDeg >= 90.0f) {
            spdlog::warn("[LightUtils] Spot light '{}' outer cone angle {:.1f} out of (0, 90) range",
                         s.name, s.cone.outerDeg);
            ok = false;
        }
        if (glm::length(s.direction) < 0.001f) {
            spdlog::warn("[LightUtils] Spot light '{}' has near-zero direction vector", s.name);
            ok = false;
        }
    }

    return ok;
}

} // namespace LightUtils
