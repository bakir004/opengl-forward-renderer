#include "scene/LightEnvironment.h"
#include <spdlog/spdlog.h>

void LightEnvironment::SetDirectionalLight(const DirectionalLight& light) {
    m_directional = light;
}

void LightEnvironment::ClearDirectionalLight() {
    m_directional.reset();
}

bool LightEnvironment::HasDirectionalLight() const {
    return m_directional.has_value();
}

const DirectionalLight& LightEnvironment::GetDirectionalLight() const {
    return m_directional.value();
}

DirectionalLight& LightEnvironment::GetDirectionalLight() {
    return m_directional.value();
}

void LightEnvironment::AddPointLight(const PointLight& light) {
    if (static_cast<int>(m_pointLights.size()) >= kMaxPointLights) {
        spdlog::warn("[LightEnvironment] Point light limit ({}) reached — '{}' dropped",
                     kMaxPointLights, light.name);
        return;
    }
    m_pointLights.push_back(light);
}

void LightEnvironment::ClearPointLights() {
    m_pointLights.clear();
}

const std::vector<PointLight>& LightEnvironment::GetPointLights() const {
    return m_pointLights;
}

std::vector<PointLight>& LightEnvironment::GetPointLights() {
    return m_pointLights;
}

void LightEnvironment::AddSpotLight(const SpotLight& light) {
    if (static_cast<int>(m_spotLights.size()) >= kMaxSpotLights) {
        spdlog::warn("[LightEnvironment] Spot light limit ({}) reached — '{}' dropped",
                     kMaxSpotLights, light.name);
        return;
    }
    m_spotLights.push_back(light);
}

void LightEnvironment::ClearSpotLights() {
    m_spotLights.clear();
}

const std::vector<SpotLight>& LightEnvironment::GetSpotLights() const {
    return m_spotLights;
}

std::vector<SpotLight>& LightEnvironment::GetSpotLights() {
    return m_spotLights;
}

int LightEnvironment::ActiveLightCount() const {
    int count = 0;
    if (m_directional && m_directional->enabled) ++count;
    for (const auto& p : m_pointLights) if (p.enabled) ++count;
    for (const auto& s : m_spotLights)  if (s.enabled) ++count;
    return count;
}

void LightEnvironment::Clear() {
    m_directional.reset();
    m_pointLights.clear();
    m_spotLights.clear();
}
