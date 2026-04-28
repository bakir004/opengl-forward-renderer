#include "core/Material.h"
#include "core/ShaderProgram.h"
#include "core/Texture2D.h"
#include <glad/glad.h>
#include <spdlog/spdlog.h>
#include <utility>

namespace {

constexpr glm::vec3 kDefaultPbrAlbedoColor(1.0f, 1.0f, 1.0f);
constexpr float kDefaultPbrMetallicValue = 0.0f;
constexpr float kDefaultPbrRoughnessValue = 0.5f;
constexpr glm::vec3 kDefaultPbrEmissiveColor(0.0f, 0.0f, 0.0f);

void SetOptionalIntUniform(GLuint programId, const char* name, int value)
{
    const GLint location = glGetUniformLocation(programId, name);
    if (location != -1)
        glUniform1i(location, value);
}

void SetOptionalFloatUniform(GLuint programId, const char* name, float value)
{
    const GLint location = glGetUniformLocation(programId, name);
    if (location != -1)
        glUniform1f(location, value);
}

void SetOptionalVec3Uniform(GLuint programId, const char* name, const glm::vec3& value)
{
    const GLint location = glGetUniformLocation(programId, name);
    if (location != -1)
        glUniform3f(location, value.x, value.y, value.z);
}

struct MaterialSchemaState {
    std::shared_ptr<Texture2D> albedoMap;
    std::shared_ptr<Texture2D> normalMap;
    std::shared_ptr<Texture2D> metallicMap;
    std::shared_ptr<Texture2D> roughnessMap;
    std::shared_ptr<Texture2D> aoMap;
    std::shared_ptr<Texture2D> emissiveMap;
    glm::vec3 albedoColor = kDefaultPbrAlbedoColor;
    float metallicValue = kDefaultPbrMetallicValue;
    float roughnessValue = kDefaultPbrRoughnessValue;
    glm::vec3 emissiveColor = kDefaultPbrEmissiveColor;
    bool hasAlbedoMap = false;
    bool hasNormalMap = false;
    bool hasMetallicMap = false;
    bool hasRoughnessMap = false;
    bool hasAoMap = false;
    bool hasEmissiveMap = false;
};

bool IsTexturePresent(const std::shared_ptr<Texture2D>& texture)
{
    return texture && texture->IsValid();
}

void BindTextureUnit(const ShaderProgram& shader,
                     const char* uniformName,
                     int textureUnit,
                     const std::shared_ptr<Texture2D>& texture)
{
    if (IsTexturePresent(texture))
        texture->Bind(static_cast<GLuint>(textureUnit));
    else
        Texture2D::Unbind(static_cast<GLuint>(textureUnit));

    SetOptionalIntUniform(shader.GetID(), uniformName, textureUnit);
}

void BindMaterialSchema(const ShaderProgram& shader, const MaterialSchemaState& state)
{
    BindTextureUnit(shader, TextureSlot::Albedo, MaterialTextureUnit::Albedo, state.albedoMap);
    BindTextureUnit(shader, TextureSlot::Normal, MaterialTextureUnit::Normal, state.normalMap);
    BindTextureUnit(shader, TextureSlot::Metallic, MaterialTextureUnit::Metallic, state.metallicMap);
    BindTextureUnit(shader, TextureSlot::Roughness, MaterialTextureUnit::Roughness, state.roughnessMap);
    BindTextureUnit(shader, TextureSlot::AO, MaterialTextureUnit::AO, state.aoMap);
    BindTextureUnit(shader, TextureSlot::Emissive, MaterialTextureUnit::Emissive, state.emissiveMap);

    shader.SetUniform("u_HasAlbedoMap", state.hasAlbedoMap);
    shader.SetUniform("u_HasNormalMap", state.hasNormalMap);
    shader.SetUniform("u_HasMetallicMap", state.hasMetallicMap);
    shader.SetUniform("u_HasRoughnessMap", state.hasRoughnessMap);
    shader.SetUniform("u_HasAoMap", state.hasAoMap);
    shader.SetUniform("u_HasEmissiveMap", state.hasEmissiveMap);

    shader.SetUniform("u_AlbedoColor", state.albedoColor);
    shader.SetUniform("u_MetallicValue", state.metallicValue);
    shader.SetUniform("u_RoughnessValue", state.roughnessValue);
    shader.SetUniform("u_EmissiveColor", state.emissiveColor);
}

void ApplyPbrFallbackUniformDefaults(const ShaderProgram& shader)
{
    const GLuint programId = shader.GetID();
    if (programId == 0)
        return;

    SetOptionalIntUniform(programId, TextureSlot::Albedo, MaterialTextureUnit::Albedo);
    SetOptionalIntUniform(programId, TextureSlot::Normal, MaterialTextureUnit::Normal);
    SetOptionalIntUniform(programId, TextureSlot::Metallic, MaterialTextureUnit::Metallic);
    SetOptionalIntUniform(programId, TextureSlot::Roughness, MaterialTextureUnit::Roughness);
    SetOptionalIntUniform(programId, TextureSlot::AO, MaterialTextureUnit::AO);
    SetOptionalIntUniform(programId, TextureSlot::Emissive, MaterialTextureUnit::Emissive);
    SetOptionalVec3Uniform(programId, "u_AlbedoColor", kDefaultPbrAlbedoColor);
    SetOptionalFloatUniform(programId, "u_MetallicValue", kDefaultPbrMetallicValue);
    SetOptionalFloatUniform(programId, "u_RoughnessValue", kDefaultPbrRoughnessValue);
    SetOptionalVec3Uniform(programId, "u_EmissiveColor", kDefaultPbrEmissiveColor);
    SetOptionalIntUniform(programId, "u_HasAlbedoMap", 0);
    SetOptionalIntUniform(programId, "u_HasNormalMap", 0);
    SetOptionalIntUniform(programId, "u_HasMetallicMap", 0);
    SetOptionalIntUniform(programId, "u_HasRoughnessMap", 0);
    SetOptionalIntUniform(programId, "u_HasAoMap", 0);
    SetOptionalIntUniform(programId, "u_HasEmissiveMap", 0);
}

} // namespace

// ---------------------------------------------------------------------------
//  Material
// ---------------------------------------------------------------------------

Material::Material(std::shared_ptr<ShaderProgram> shader)
    : m_shader(std::move(shader))
{
}

std::shared_ptr<Material> Material::CreateDefaultMaterial(std::shared_ptr<ShaderProgram> shader)
{
    auto material = std::make_shared<Material>(std::move(shader));
    material->m_albedoColor = {0.5f, 0.5f, 0.5f};
    material->m_metallicValue = 0.0f;
    material->m_roughnessValue = 0.5f;
    material->m_emissiveColor = {0.0f, 0.0f, 0.0f};
    return material;
}

std::shared_ptr<Material> Material::CreateMetalMaterial(std::shared_ptr<ShaderProgram> shader,
                                                        glm::vec3 color,
                                                        float roughness)
{
    auto material = std::make_shared<Material>(std::move(shader));
    material->m_albedoColor = color;
    material->m_metallicValue = 1.0f;
    material->m_roughnessValue = roughness;
    return material;
}

std::shared_ptr<Material> Material::CreateDielectricMaterial(std::shared_ptr<ShaderProgram> shader,
                                                             glm::vec3 color,
                                                             float roughness)
{
    auto material = std::make_shared<Material>(std::move(shader));
    material->m_albedoColor = color;
    material->m_metallicValue = 0.0f;
    material->m_roughnessValue = roughness;
    return material;
}

Material& Material::SetFloat(const std::string& name, float value) {
    if (name == "u_MetallicValue" || name == "u_Metallic") {
        m_metallicValue = value;
        return *this;
    }
    if (name == "u_RoughnessValue" || name == "u_Roughness") {
        m_roughnessValue = value;
        return *this;
    }
    m_floats[name] = value;
    return *this;
}

Material& Material::SetVec3(const std::string& name, glm::vec3 value) {
    if (name == "u_AlbedoColor") {
        m_albedoColor = value;
        return *this;
    }
    if (name == "u_EmissiveColor") {
        m_emissiveColor = value;
        return *this;
    }
    m_vec3s[name] = value;
    return *this;
}

Material& Material::SetVec4(const std::string& name, glm::vec4 value) {
    m_vec4s[name] = value;
    return *this;
}

Material& Material::SetTexture(const std::string& slotName, std::shared_ptr<Texture2D> texture) {
    const bool present = IsTexturePresent(texture);
    if (slotName == TextureSlot::Albedo) {
        m_albedoMap = std::move(texture);
        m_hasAlbedoMap = present;
        return *this;
    }
    if (slotName == TextureSlot::Normal) {
        m_normalMap = std::move(texture);
        m_hasNormalMap = present;
        return *this;
    }
    if (slotName == TextureSlot::Metallic) {
        m_metallicMap = std::move(texture);
        m_hasMetallicMap = present;
        return *this;
    }
    if (slotName == TextureSlot::Roughness) {
        m_roughnessMap = std::move(texture);
        m_hasRoughnessMap = present;
        return *this;
    }
    if (slotName == TextureSlot::AO) {
        m_aoMap = std::move(texture);
        m_hasAoMap = present;
        return *this;
    }
    if (slotName == TextureSlot::Emissive) {
        m_emissiveMap = std::move(texture);
        m_hasEmissiveMap = present;
        return *this;
    }
    return *this;
}

void Material::Bind() const {
    if (!m_shader) return;
    m_shader->Bind();
    ApplyPbrFallbackUniformDefaults(*m_shader);

    MaterialSchemaState state;
    state.albedoMap = m_albedoMap;
    state.normalMap = m_normalMap;
    state.metallicMap = m_metallicMap;
    state.roughnessMap = m_roughnessMap;
    state.aoMap = m_aoMap;
    state.emissiveMap = m_emissiveMap;
    state.albedoColor = m_albedoColor;
    state.metallicValue = m_metallicValue;
    state.roughnessValue = m_roughnessValue;
    state.emissiveColor = m_emissiveColor;
    state.hasAlbedoMap = m_hasAlbedoMap;
    state.hasNormalMap = m_hasNormalMap;
    state.hasMetallicMap = m_hasMetallicMap;
    state.hasRoughnessMap = m_hasRoughnessMap;
    state.hasAoMap = m_hasAoMap;
    state.hasEmissiveMap = m_hasEmissiveMap;
    BindMaterialSchema(*m_shader, state);

    for (const auto& [name, val] : m_floats)  m_shader->SetUniform(name, val);
    for (const auto& [name, val] : m_vec3s)   m_shader->SetUniform(name, val);
    for (const auto& [name, val] : m_vec4s)   m_shader->SetUniform(name, val);
}

void Material::Unbind() const {
    ShaderProgram::Unbind();
}

// ---------------------------------------------------------------------------
//  MaterialInstance
// ---------------------------------------------------------------------------

MaterialInstance::MaterialInstance(std::shared_ptr<Material> parent)
    : m_parent(std::move(parent))
{
}

MaterialInstance& MaterialInstance::SetFloat(const std::string& name, float value) {
    if (name == "u_MetallicValue" || name == "u_Metallic") {
        m_metallicValue = value;
        return *this;
    }
    if (name == "u_RoughnessValue" || name == "u_Roughness") {
        m_roughnessValue = value;
        return *this;
    }
    m_floats[name] = value;
    return *this;
}

MaterialInstance& MaterialInstance::SetVec3(const std::string& name, glm::vec3 value) {
    if (name == "u_AlbedoColor") {
        m_albedoColor = value;
        return *this;
    }
    if (name == "u_EmissiveColor") {
        m_emissiveColor = value;
        return *this;
    }
    m_vec3s[name] = value;
    return *this;
}

MaterialInstance& MaterialInstance::SetVec4(const std::string& name, glm::vec4 value) {
    m_vec4s[name] = value;
    return *this;
}

MaterialInstance& MaterialInstance::SetTexture(const std::string& slotName,
                                                std::shared_ptr<Texture2D> texture) {
    if (slotName == TextureSlot::Albedo) {
        m_albedoMap = std::move(texture);
        return *this;
    }
    if (slotName == TextureSlot::Normal) {
        m_normalMap = std::move(texture);
        return *this;
    }
    if (slotName == TextureSlot::Metallic) {
        m_metallicMap = std::move(texture);
        return *this;
    }
    if (slotName == TextureSlot::Roughness) {
        m_roughnessMap = std::move(texture);
        return *this;
    }
    if (slotName == TextureSlot::AO) {
        m_aoMap = std::move(texture);
        return *this;
    }
    if (slotName == TextureSlot::Emissive) {
        m_emissiveMap = std::move(texture);
        return *this;
    }
    return *this;
}

void MaterialInstance::Bind() const {
    if (!m_parent) return;

    ShaderProgram* shader = m_parent->GetShader();
    if (!shader) return;

    shader->Bind();
    ApplyPbrFallbackUniformDefaults(*shader);

    MaterialSchemaState state;
    state.albedoMap = m_parent->m_albedoMap;
    state.normalMap = m_parent->m_normalMap;
    state.metallicMap = m_parent->m_metallicMap;
    state.roughnessMap = m_parent->m_roughnessMap;
    state.aoMap = m_parent->m_aoMap;
    state.emissiveMap = m_parent->m_emissiveMap;
    state.albedoColor = m_parent->m_albedoColor;
    state.metallicValue = m_parent->m_metallicValue;
    state.roughnessValue = m_parent->m_roughnessValue;
    state.emissiveColor = m_parent->m_emissiveColor;
    state.hasAlbedoMap = m_parent->m_hasAlbedoMap;
    state.hasNormalMap = m_parent->m_hasNormalMap;
    state.hasMetallicMap = m_parent->m_hasMetallicMap;
    state.hasRoughnessMap = m_parent->m_hasRoughnessMap;
    state.hasAoMap = m_parent->m_hasAoMap;
    state.hasEmissiveMap = m_parent->m_hasEmissiveMap;

    if (m_albedoMap.has_value()) {
        state.albedoMap = *m_albedoMap;
        state.hasAlbedoMap = IsTexturePresent(state.albedoMap);
    }
    if (m_normalMap.has_value()) {
        state.normalMap = *m_normalMap;
        state.hasNormalMap = IsTexturePresent(state.normalMap);
    }
    if (m_metallicMap.has_value()) {
        state.metallicMap = *m_metallicMap;
        state.hasMetallicMap = IsTexturePresent(state.metallicMap);
    }
    if (m_roughnessMap.has_value()) {
        state.roughnessMap = *m_roughnessMap;
        state.hasRoughnessMap = IsTexturePresent(state.roughnessMap);
    }
    if (m_aoMap.has_value()) {
        state.aoMap = *m_aoMap;
        state.hasAoMap = IsTexturePresent(state.aoMap);
    }
    if (m_emissiveMap.has_value()) {
        state.emissiveMap = *m_emissiveMap;
        state.hasEmissiveMap = IsTexturePresent(state.emissiveMap);
    }
    if (m_albedoColor.has_value())
        state.albedoColor = *m_albedoColor;
    if (m_metallicValue.has_value())
        state.metallicValue = *m_metallicValue;
    if (m_roughnessValue.has_value())
        state.roughnessValue = *m_roughnessValue;
    if (m_emissiveColor.has_value())
        state.emissiveColor = *m_emissiveColor;

    BindMaterialSchema(*shader, state);

    for (const auto& [name, val] : m_parent->m_floats) shader->SetUniform(name, val);
    for (const auto& [name, val] : m_parent->m_vec3s)  shader->SetUniform(name, val);
    for (const auto& [name, val] : m_parent->m_vec4s)  shader->SetUniform(name, val);
    for (const auto& [name, val] : m_floats)  shader->SetUniform(name, val);
    for (const auto& [name, val] : m_vec3s)   shader->SetUniform(name, val);
    for (const auto& [name, val] : m_vec4s)   shader->SetUniform(name, val);
}

const ShaderProgram* MaterialInstance::GetShader() const {
    return m_parent ? m_parent->GetShader() : nullptr;
}

ShaderProgram* MaterialInstance::GetShader() {
    return m_parent ? m_parent->GetShader() : nullptr;
}
