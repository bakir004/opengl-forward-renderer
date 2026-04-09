#pragma once

#include "scene/Light.h"
#include "scene/LightEnvironment.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace LightUtils {

/// Returns the effective radiant color of a light (color * intensity).
inline glm::vec3 Irradiance(const DirectionalLight& l) { return l.color * l.intensity; }
inline glm::vec3 Irradiance(const PointLight& l)       { return l.color * l.intensity; }
inline glm::vec3 Irradiance(const SpotLight& l)        { return l.color * l.intensity; }

/// Evaluates the attenuation factor at distance d.
/// Returns a value in [0, 1]. Reaches exactly 0 at light.radius.
inline float EvaluateAttenuation(const Attenuation& a, float d, float radius) {
    const float rawAtten = 1.0f / (a.constant + a.linear * d + a.quadratic * d * d);
    // Smooth window function to guarantee zero at radius.
    const float window = std::max(0.0f, 1.0f - (d / radius) * (d / radius));
    return rawAtten * window * window;
}

/// Returns the inner and outer half-angles in radians for a SpotLight.
inline float SpotInnerCosine(const SpotLight& l) {
    return std::cos(glm::radians(l.cone.innerDeg));
}
inline float SpotOuterCosine(const SpotLight& l) {
    return std::cos(glm::radians(l.cone.outerDeg));
}

/// Validates a LightEnvironment and logs any issues.
/// Returns true if the environment is usable (warnings are non-fatal).
bool Validate(const LightEnvironment& env);

/// Builds the view-projection matrix for shadow map rendering of a directional light.
/// @param light     The directional light.
/// @param sceneMid  World-space center of the scene (used to position the light camera).
/// @param sceneRadius  Half-diagonal of the scene AABB — sets ortho extents.
inline glm::mat4 DirectionalLightViewProjection(const DirectionalLight& light,
                                                glm::vec3 sceneMid,
                                                float sceneRadius)
{
    const glm::vec3 dir   = glm::normalize(light.direction);
    const glm::vec3 eye   = sceneMid - dir * sceneRadius;
    const glm::vec3 up    = (std::abs(glm::dot(dir, {0,1,0})) < 0.99f)
                            ? glm::vec3(0,1,0) : glm::vec3(1,0,0);

    const glm::mat4 view  = glm::lookAt(eye, sceneMid, up);
    const glm::mat4 proj  = glm::ortho(
        -sceneRadius, sceneRadius,
        -sceneRadius, sceneRadius,
        light.shadow.nearPlane,
        light.shadow.nearPlane + sceneRadius * 2.0f + light.shadow.farPlane);

    return proj * view;
}

/// Builds the view-projection matrix for a spot light shadow map.
inline glm::mat4 SpotLightViewProjection(const SpotLight& light) {
    const glm::vec3 up   = (std::abs(glm::dot(glm::normalize(light.direction), {0,1,0})) < 0.99f)
                           ? glm::vec3(0,1,0) : glm::vec3(1,0,0);
    const glm::mat4 view = glm::lookAt(light.position,
                                       light.position + glm::normalize(light.direction),
                                       up);
    const float fovY     = glm::radians(light.cone.outerDeg * 2.0f);
    const glm::mat4 proj = glm::perspective(fovY, 1.0f,
                                            light.shadow.nearPlane,
                                            light.shadow.farPlane);
    return proj * view;
}

} // namespace LightUtils
