#pragma once

#include "scene/Light.h"
#include <vector>
#include <optional>

/// Maximum light counts enforced at submission time.
/// Person 2 (GPU packing) must keep the UBO array limits in sync with these.
static constexpr int kMaxPointLights = 16;
static constexpr int kMaxSpotLights  = 8;

/// Scene-facing light container.
///
/// Scene subclasses populate this by calling the Add* helpers, then pass it
/// to FrameSubmission. Person 3 copies it into the submission, Person 4
/// uploads it to the GPU, Person 5/6 consume it in shaders.
///
/// Only one directional light is supported per frame. Scenes may omit it.
class LightEnvironment {
public:
    /// Ambient radiance applied to all objects regardless of other lights.
    glm::vec3 ambientColor     = glm::vec3(0.03f);
    float     ambientIntensity = 1.0f;

    void SetDirectionalLight(const DirectionalLight& light);
    void ClearDirectionalLight();
    [[nodiscard]] bool HasDirectionalLight() const;
    [[nodiscard]] const DirectionalLight& GetDirectionalLight() const;
    [[nodiscard]] DirectionalLight& GetDirectionalLight();

    /// Adds a point light. Silently ignored if kMaxPointLights is reached.
    void AddPointLight(const PointLight& light);
    void ClearPointLights();
    [[nodiscard]] const std::vector<PointLight>& GetPointLights() const;
    [[nodiscard]] std::vector<PointLight>&       GetPointLights();

    /// Adds a spot light. Silently ignored if kMaxSpotLights is reached.
    void AddSpotLight(const SpotLight& light);
    void ClearSpotLights();
    [[nodiscard]] const std::vector<SpotLight>& GetSpotLights() const;
    [[nodiscard]] std::vector<SpotLight>&       GetSpotLights();

    /// Returns the total number of enabled lights of all types.
    [[nodiscard]] int ActiveLightCount() const;

    /// Resets to default state: no directional, no point, no spot lights.
    void Clear();

    /// Returns ambient irradiance (color * intensity) for use in shaders.
    [[nodiscard]] glm::vec3 AmbientIrradiance() const {
        return ambientColor * ambientIntensity;
    }

private:
    std::optional<DirectionalLight> m_directional;
    std::vector<PointLight>         m_pointLights;
    std::vector<SpotLight>          m_spotLights;
};
