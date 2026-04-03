#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <unordered_map>

class ShaderProgram;
class Texture2D;

/// Texture slot names understood by the PBR forward shader.
/// These map directly to the sampler uniform names in GLSL.
namespace TextureSlot {
    inline constexpr const char* Albedo    = "u_AlbedoMap";
    inline constexpr const char* Normal    = "u_NormalMap";
    inline constexpr const char* Metallic  = "u_MetallicMap";
    inline constexpr const char* Roughness = "u_RoughnessMap";
    inline constexpr const char* AO        = "u_AOMap";
    inline constexpr const char* Emissive  = "u_EmissiveMap";
}

/// Immutable description of a material type.
///
/// A Material owns a shader and a set of default parameter values.
/// Applications create MaterialInstances from it to override parameters
/// per-object without duplicating the shader.
class Material {
public:
    explicit Material(std::shared_ptr<ShaderProgram> shader);

    /// Sets a named float parameter default.
    Material& SetFloat(const std::string& name, float value);

    /// Sets a named vec3 parameter default.
    Material& SetVec3(const std::string& name, glm::vec3 value);

    /// Sets a named vec4 parameter default.
    Material& SetVec4(const std::string& name, glm::vec4 value);

    /// Assigns a texture to a named slot (e.g. TextureSlot::Albedo).
    Material& SetTexture(const std::string& slotName, std::shared_ptr<Texture2D> texture);

    [[nodiscard]] const ShaderProgram* GetShader() const { return m_shader.get(); }
    [[nodiscard]] ShaderProgram*       GetShader()       { return m_shader.get(); }

    /// Binds the shader and uploads all default parameters + textures.
    /// Called by MaterialInstance::Bind before per-instance overrides.
    void Bind() const;

    /// Unbinds the shader.
    void Unbind() const;

private:
    std::shared_ptr<ShaderProgram> m_shader;

    std::unordered_map<std::string, float>      m_floats;
    std::unordered_map<std::string, glm::vec3>  m_vec3s;
    std::unordered_map<std::string, glm::vec4>  m_vec4s;

    // Ordered so we can assign consistent GL texture units.
    // Key = uniform name (e.g. "u_AlbedoMap"), value = texture.
    std::unordered_map<std::string, std::shared_ptr<Texture2D>> m_textures;

    friend class MaterialInstance;
};

/// Per-object material override layer.
///
/// Inherits all parameters from its parent Material but can override any
/// scalar, vector, or texture value for this specific instance.
/// Keeps a non-owning pointer to the parent — the parent must outlive all instances.
class MaterialInstance {
public:
    explicit MaterialInstance(std::shared_ptr<Material> parent);

    /// Overrides a float parameter for this instance.
    MaterialInstance& SetFloat(const std::string& name, float value);

    /// Overrides a vec3 parameter for this instance.
    MaterialInstance& SetVec3(const std::string& name, glm::vec3 value);

    /// Overrides a vec4 parameter for this instance.
    MaterialInstance& SetVec4(const std::string& name, glm::vec4 value);

    /// Overrides a texture slot for this instance.
    MaterialInstance& SetTexture(const std::string& slotName, std::shared_ptr<Texture2D> texture);

    /// Binds the parent material then applies instance overrides.
    /// Assigns GL texture units 0..N in slot insertion order.
    void Bind() const;

    /// Returns the underlying shader (from parent).
    [[nodiscard]] const ShaderProgram* GetShader() const;
    [[nodiscard]] ShaderProgram*       GetShader();

private:
    std::shared_ptr<Material> m_parent;

    std::unordered_map<std::string, float>      m_floats;
    std::unordered_map<std::string, glm::vec3>  m_vec3s;
    std::unordered_map<std::string, glm::vec4>  m_vec4s;
    std::unordered_map<std::string, std::shared_ptr<Texture2D>> m_textures;
};
