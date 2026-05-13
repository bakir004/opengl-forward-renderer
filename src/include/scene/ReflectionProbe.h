#pragma once

#include <glm/glm.hpp>
#include <memory>

class Texture2D;
class TextureCubemap;

/// Texture slot names and units reserved for image-based lighting resources.
/// Units 0-6 are material textures and unit 7 is the cascaded shadow map array.
namespace EnvironmentTextureSlot {
    inline constexpr const char* Irradiance = "u_IrradianceMap";
    inline constexpr const char* Prefiltered = "u_PrefilteredMap";
    inline constexpr const char* BrdfLut = "u_BRDFLUT";
}

namespace EnvironmentTextureUnit {
    inline constexpr int Irradiance = 8;
    inline constexpr int Prefiltered = 9;
    /// Split-sum BRDF LUT (RG16F); same data for every probe — baked once in EnvironmentLightingPipeline.
    inline constexpr int BrdfLut = 10;
}

/// Influence volume shape used by a reflection probe.
enum class ReflectionProbeInfluenceType {
    Global = 0,
    Sphere = 1,
    Box = 2,
};

/// Scene-facing reflection probe data consumed by the renderer.
///
/// Sprint 9 tasks 1-4 can fill irradianceCubemap, prefilteredCubemap, and brdfLut
/// after runtime generation. Task 9 can extend the influence selection without
/// changing the material binding path.
struct ReflectionProbe {
    std::shared_ptr<TextureCubemap> sourceCubemap;
    std::shared_ptr<TextureCubemap> irradianceCubemap;
    std::shared_ptr<TextureCubemap> prefilteredCubemap;
    std::shared_ptr<Texture2D> brdfLut;

    float intensity = 1.0f;
    ReflectionProbeInfluenceType influenceType = ReflectionProbeInfluenceType::Global;
    glm::vec3 position{0.0f};
    float radius = 0.0f;
    glm::vec3 boxMin{-1.0f};
    glm::vec3 boxMax{1.0f};

    [[nodiscard]] bool HasDiffuseIbl() const { return static_cast<bool>(irradianceCubemap); }
    [[nodiscard]] bool HasSpecularIbl() const { return static_cast<bool>(prefilteredCubemap) && static_cast<bool>(brdfLut); }
    [[nodiscard]] bool HasAnyIbl() const { return HasDiffuseIbl() || HasSpecularIbl(); }
};
