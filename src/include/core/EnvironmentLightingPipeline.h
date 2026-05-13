#pragma once

#include "scene/ReflectionProbe.h"

#include <memory>

class TextureCubemap;

/// Runtime generator for the global IBL resource set.
///
/// The pipeline owns the GPU-side capture resources required to turn a source
/// environment cubemap into the diffuse irradiance cubemap, prefiltered specular
/// cubemap, and BRDF LUT used by PBR shaders.
class EnvironmentLightingPipeline {
public:
    EnvironmentLightingPipeline();
    ~EnvironmentLightingPipeline();

    EnvironmentLightingPipeline(const EnvironmentLightingPipeline&) = delete;
    EnvironmentLightingPipeline& operator=(const EnvironmentLightingPipeline&) = delete;

    EnvironmentLightingPipeline(EnvironmentLightingPipeline&&) noexcept;
    EnvironmentLightingPipeline& operator=(EnvironmentLightingPipeline&&) noexcept;

    /// Initializes the capture shaders, meshes, and framebuffer objects.
    bool Initialize();

    /// Releases pipeline-owned GPU resources.
    void Shutdown();

    /// Ensures the probe has a valid source cubemap and runtime-generated IBL resources.
    /// If fallbackSource is provided and the probe does not already have a source,
    /// the fallback is adopted before generation.
    [[nodiscard]] bool EnsureProbe(ReflectionProbe& probe,
                                   std::shared_ptr<TextureCubemap> fallbackSource = nullptr);

    [[nodiscard]] bool IsInitialized() const { return m_impl != nullptr; }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};