#pragma once

#include "scene/Light.h"
#include <glm/glm.hpp>
#include <string>

/// Fluent builder for DirectionalLight.
///
/// Usage:
///   auto sun = DirectionalLightBuilder()
///       .Direction({-0.5f, -1.0f, -0.3f})
///       .Color({1.0f, 0.95f, 0.85f})
///       .Intensity(1.2f)
///       .CastShadow(true)
///       .Name("Sun")
///       .Build();
class DirectionalLightBuilder {
public:
    DirectionalLightBuilder& Direction(glm::vec3 dir) {
        m_light.direction = glm::normalize(dir);
        return *this;
    }
    DirectionalLightBuilder& Color(glm::vec3 color) {
        m_light.color = color;
        return *this;
    }
    DirectionalLightBuilder& Intensity(float v) {
        m_light.intensity = v;
        return *this;
    }
    DirectionalLightBuilder& Enabled(bool v) {
        m_light.enabled = v;
        return *this;
    }
    DirectionalLightBuilder& CastShadow(bool v) {
        m_light.shadow.castShadow = v;
        return *this;
    }
    DirectionalLightBuilder& ShadowResolution(int w, int h) {
        m_light.shadow.shadowMapWidth  = w;
        m_light.shadow.shadowMapHeight = h;
        return *this;
    }
    DirectionalLightBuilder& ShadowBias(float depthBias, float normalBias = 0.02f) {
        m_light.shadow.depthBias   = depthBias;
        m_light.shadow.normalBias  = normalBias;
        return *this;
    }
    DirectionalLightBuilder& ShadowPlanes(float nearP, float farP) {
        m_light.shadow.nearPlane = nearP;
        m_light.shadow.farPlane  = farP;
        return *this;
    }
    DirectionalLightBuilder& PCFRadius(int r) {
        m_light.shadow.pcfRadius = r;
        return *this;
    }
    DirectionalLightBuilder& Name(std::string name) {
        m_light.name = std::move(name);
        return *this;
    }
    [[nodiscard]] DirectionalLight Build() const { return m_light; }

private:
    DirectionalLight m_light;
};

/// Fluent builder for PointLight.
///
/// Usage:
///   auto lamp = PointLightBuilder()
///       .Position({3.0f, 2.0f, 0.0f})
///       .Color({1.0f, 0.8f, 0.5f})
///       .Intensity(2.0f)
///       .Radius(12.0f)
///       .Name("WarmLamp")
///       .Build();
class PointLightBuilder {
public:
    PointLightBuilder& Position(glm::vec3 pos) {
        m_light.position = pos;
        return *this;
    }
    PointLightBuilder& Color(glm::vec3 color) {
        m_light.color = color;
        return *this;
    }
    PointLightBuilder& Intensity(float v) {
        m_light.intensity = v;
        return *this;
    }
    PointLightBuilder& Radius(float r) {
        m_light.radius = r;
        return *this;
    }
    PointLightBuilder& AttenuationCoeffs(float constant, float linear, float quadratic) {
        m_light.attenuation = {constant, linear, quadratic};
        return *this;
    }
    PointLightBuilder& Enabled(bool v) {
        m_light.enabled = v;
        return *this;
    }
    PointLightBuilder& CastShadow(bool v) {
        m_light.shadow.castShadow = v;
        return *this;
    }
    PointLightBuilder& ShadowBias(float depthBias, float normalBias = 0.02f) {
        m_light.shadow.depthBias  = depthBias;
        m_light.shadow.normalBias = normalBias;
        return *this;
    }
    PointLightBuilder& Name(std::string name) {
        m_light.name = std::move(name);
        return *this;
    }
    [[nodiscard]] PointLight Build() const { return m_light; }

private:
    PointLight m_light;
};

/// Fluent builder for SpotLight.
class SpotLightBuilder {
public:
    SpotLightBuilder& Position(glm::vec3 pos) {
        m_light.position = pos;
        return *this;
    }
    SpotLightBuilder& Direction(glm::vec3 dir) {
        m_light.direction = glm::normalize(dir);
        return *this;
    }
    SpotLightBuilder& Color(glm::vec3 color) {
        m_light.color = color;
        return *this;
    }
    SpotLightBuilder& Intensity(float v) {
        m_light.intensity = v;
        return *this;
    }
    SpotLightBuilder& Radius(float r) {
        m_light.radius = r;
        return *this;
    }
    SpotLightBuilder& Cone(float innerDeg, float outerDeg) {
        m_light.cone = {innerDeg, outerDeg};
        return *this;
    }
    SpotLightBuilder& AttenuationCoeffs(float constant, float linear, float quadratic) {
        m_light.attenuation = {constant, linear, quadratic};
        return *this;
    }
    SpotLightBuilder& Enabled(bool v) {
        m_light.enabled = v;
        return *this;
    }
    SpotLightBuilder& CastShadow(bool v) {
        m_light.shadow.castShadow = v;
        return *this;
    }
    SpotLightBuilder& ShadowBias(float depthBias, float normalBias = 0.02f) {
        m_light.shadow.depthBias  = depthBias;
        m_light.shadow.normalBias = normalBias;
        return *this;
    }
    SpotLightBuilder& ShadowPlanes(float nearP, float farP) {
        m_light.shadow.nearPlane = nearP;
        m_light.shadow.farPlane  = farP;
        return *this;
    }
    SpotLightBuilder& PCFRadius(int r) {
        m_light.shadow.pcfRadius = r;
        return *this;
    }
    SpotLightBuilder& Name(std::string name) {
        m_light.name = std::move(name);
        return *this;
    }
    [[nodiscard]] SpotLight Build() const { return m_light; }

private:
    SpotLight m_light;
};
