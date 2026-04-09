#pragma once

#include <glm/glm.hpp>
#include <string>

enum class LightType : uint32_t {
    Directional = 0,
    Point       = 1,
    Spot        = 2,
};

/// Attenuation coefficients for distance-based falloff (point and spot lights).
/// Physical attenuation: 1 / (constant + linear * d + quadratic * d^2)
struct Attenuation {
    float constant  = 1.0f;
    float linear    = 0.09f;
    float quadratic = 0.032f;
};

/// Spot cone parameters. All angles in degrees.
struct SpotCone {
    float innerDeg = 12.5f;   ///< Full-intensity cone — no falloff inside this angle
    float outerDeg = 17.5f;   ///< Zero-intensity cutoff — full falloff outside this angle
};

/// Shadow configuration carried per-light.
/// Consumed by Person 7 (shadow resources) and Person 8 (shadow sampling).
struct LightShadowParams {
    bool  castShadow      = false;
    float depthBias       = 0.005f;   ///< Constant bias added to shadow depth comparison
    float normalBias      = 0.02f;    ///< Normal-offset bias in world units
    float nearPlane       = 0.1f;     ///< Shadow camera near clip
    float farPlane        = 100.0f;   ///< Shadow camera far clip (directional: ortho half-extent)
    int   shadowMapWidth  = 2048;
    int   shadowMapHeight = 2048;
    int   pcfRadius       = 1;        ///< PCF kernel half-size (0 = hard, 1 = 3x3, 2 = 5x5, ...)
};

/// A directional light — infinite distance, uniform direction, no position.
struct DirectionalLight {
    glm::vec3         direction  = glm::normalize(glm::vec3(-0.3f, -1.0f, -0.3f));
    float             intensity  = 1.0f;
    glm::vec3         color      = glm::vec3(1.0f);
    bool              enabled    = true;
    LightShadowParams shadow;
    std::string       name;
};

/// A point light — omnidirectional, positioned in world space.
struct PointLight {
    glm::vec3         position   = glm::vec3(0.0f);
    float             intensity  = 1.0f;
    glm::vec3         color      = glm::vec3(1.0f);
    float             radius     = 10.0f;   ///< Maximum influence radius; attenuation reaches zero at this distance
    bool              enabled    = true;
    Attenuation       attenuation;
    LightShadowParams shadow;
    std::string       name;
};

/// A spot light — cone-shaped, positioned and directed.
struct SpotLight {
    glm::vec3         position   = glm::vec3(0.0f);
    glm::vec3         direction  = glm::normalize(glm::vec3(0.0f, -1.0f, 0.0f));
    float             intensity  = 1.0f;
    glm::vec3         color      = glm::vec3(1.0f);
    float             radius     = 20.0f;
    bool              enabled    = true;
    Attenuation       attenuation;
    SpotCone          cone;
    LightShadowParams shadow;
    std::string       name;
};
