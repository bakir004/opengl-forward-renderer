#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <optional>
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
    inline constexpr const char* SpecularGlossiness = "u_SpecularGlossinessMap";
}

/// Fixed texture units reserved for the forward PBR material schema.
/// Keep these values in sync with the GLSL sampler uniforms in mesh.frag:
///   - albedoMap   -> unit 0
///   - normalMap   -> unit 1
///   - metallicMap -> unit 2
///   - roughnessMap-> unit 3
///   - aoMap       -> unit 4
///   - emissiveMap -> unit 5
///   - specularGlossiness -> unit 6
/// Environment / IBL (see EnvironmentTextureUnit in ReflectionProbe.h):
///   - irradiance / prefiltered cubemaps -> 8, 9; BRDF LUT -> 10
namespace MaterialTextureUnit {
    inline constexpr int Albedo    = 0;
    inline constexpr int Normal    = 1;
    inline constexpr int Metallic  = 2;
    inline constexpr int Roughness = 3;
    inline constexpr int AO        = 4;
    inline constexpr int Emissive  = 5;
    inline constexpr int SpecularGlossiness = 6;
}

/// Immutable description of a material type.
///
/// A Material owns a shader and a set of default parameter values.
/// Applications create MaterialInstances from it to override parameters
/// per-object without duplicating the shader.
class Material {
public:
    explicit Material(std::shared_ptr<ShaderProgram> shader);

    static std::shared_ptr<Material> CreateDefaultMaterial(std::shared_ptr<ShaderProgram> shader);
    static std::shared_ptr<Material> CreateMetalMaterial(std::shared_ptr<ShaderProgram> shader,
                                                         glm::vec3 color,
                                                         float roughness);
    static std::shared_ptr<Material> CreateDielectricMaterial(std::shared_ptr<ShaderProgram> shader,
                                                              glm::vec3 color,
                                                              float roughness);

    /// Sets a named float parameter default.
    Material& SetFloat(const std::string& name, float value);

    /// Sets a named vec3 parameter default.
    Material& SetVec3(const std::string& name, glm::vec3 value);

    /// Sets a named vec4 parameter default.
    Material& SetVec4(const std::string& name, glm::vec4 value);

    /// Assigns a texture to a named slot (e.g. TextureSlot::Albedo).
    Material& SetTexture(const std::string& slotName, std::shared_ptr<Texture2D> texture);

    void               SetName(const std::string& name) { m_name = name; }
    [[nodiscard]] const std::string& GetName() const { return m_name; }

    [[nodiscard]] const ShaderProgram* GetShader() const { return m_shader.get(); }
    [[nodiscard]] ShaderProgram*       GetShader()       { return m_shader.get(); }

    /// Binds the shader and uploads all default parameters + textures.
    /// Called by MaterialInstance::Bind before per-instance overrides.
    void Bind() const;

    /// Unbinds the shader.
    void Unbind() const;

private:
    std::shared_ptr<ShaderProgram> m_shader;

    std::shared_ptr<Texture2D> m_albedoMap;
    std::shared_ptr<Texture2D> m_normalMap;
    std::shared_ptr<Texture2D> m_metallicMap;
    std::shared_ptr<Texture2D> m_roughnessMap;
    std::shared_ptr<Texture2D> m_aoMap;
    std::shared_ptr<Texture2D> m_emissiveMap;
    std::shared_ptr<Texture2D> m_specularGlossinessMap;

    glm::vec3 m_albedoColor{1.0f, 1.0f, 1.0f};
    float     m_metallicValue  = 0.0f;
    float     m_roughnessValue = 0.5f;
    glm::vec3 m_emissiveColor{0.0f, 0.0f, 0.0f};
    float     m_emissiveStrength = 1.0f;
    float     m_aoStrength     = 1.0f;
    float     m_normalScale    = 1.0f;
    bool      m_useNormalMap   = true;

    bool m_hasAlbedoMap    = false;
    bool m_hasNormalMap    = false;
    bool m_hasMetallicMap  = false;
    bool m_hasRoughnessMap = false;
    bool m_hasAoMap        = false;
    bool m_hasEmissiveMap  = false;
    bool m_hasSpecularGlossinessMap = false;

    std::unordered_map<std::string, float>      m_floats;
    std::unordered_map<std::string, bool>       m_bools;
    std::unordered_map<std::string, glm::vec3>  m_vec3s;
    std::unordered_map<std::string, glm::vec4>  m_vec4s;

    std::string m_name;

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

    /// Overrides a boolean parameter for this instance.
    MaterialInstance& SetBool(const std::string& name, bool value);

    /// Overrides a texture slot for this instance.
    MaterialInstance& SetTexture(const std::string& slotName, std::shared_ptr<Texture2D> texture);

    [[nodiscard]] float     GetFloat(const std::string& name, float defaultValue = 0.0f) const;
    [[nodiscard]] glm::vec3 GetVec3(const std::string& name, glm::vec3 defaultValue = glm::vec3(0.0f)) const;
    [[nodiscard]] bool      GetUseNormalMap() const;
    void                    SetUseNormalMap(bool use);

    /// Binds the parent material then applies instance overrides.
    /// Assigns GL texture units 0..N in slot insertion order.
    void Bind() const;

    void               SetName(const std::string& name) { m_name = name; }
    [[nodiscard]] const std::string& GetName() const { return m_name.empty() ? m_parent->GetName() : m_name; }

    /// Returns the underlying shader (from parent).
    [[nodiscard]] const ShaderProgram* GetShader() const;
    [[nodiscard]] ShaderProgram*       GetShader();

private:
    std::shared_ptr<Material> m_parent;

    std::optional<std::shared_ptr<Texture2D>> m_albedoMap;
    std::optional<std::shared_ptr<Texture2D>> m_normalMap;
    std::optional<std::shared_ptr<Texture2D>> m_metallicMap;
    std::optional<std::shared_ptr<Texture2D>> m_roughnessMap;
    std::optional<std::shared_ptr<Texture2D>> m_aoMap;
    std::optional<std::shared_ptr<Texture2D>> m_emissiveMap;
    std::optional<std::shared_ptr<Texture2D>> m_specularGlossinessMap;

    std::optional<glm::vec3> m_albedoColor;
    std::optional<float>     m_metallicValue;
    std::optional<float>     m_roughnessValue;
    std::optional<glm::vec3> m_emissiveColor;
    std::optional<float>     m_emissiveStrength;
    std::optional<float>     m_aoStrength;
    std::optional<float>     m_normalScale;
    std::optional<bool>      m_useNormalMap;

    std::unordered_map<std::string, float>      m_floats;
    std::unordered_map<std::string, bool>       m_bools;
    std::unordered_map<std::string, glm::vec3>  m_vec3s;
    std::unordered_map<std::string, glm::vec4>  m_vec4s;

    std::string m_name;
};
