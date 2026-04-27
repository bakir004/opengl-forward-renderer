#include "core/Material.h"
#include "core/ShaderProgram.h"
#include "core/Texture2D.h"
#include <glad/glad.h>
#include <spdlog/spdlog.h>

namespace {

constexpr glm::vec3 kDefaultPbrAlbedoColor(0.5f, 0.5f, 0.5f);
constexpr float kDefaultPbrMetallicValue = 0.0f;
constexpr float kDefaultPbrRoughnessValue = 0.5f;

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

void ApplyPbrFallbackUniformDefaults(const ShaderProgram& shader)
{
    const GLuint programId = shader.GetID();
    if (programId == 0)
        return;

    SetOptionalVec3Uniform(programId, "u_AlbedoColor", kDefaultPbrAlbedoColor);
    SetOptionalFloatUniform(programId, "u_MetallicValue", kDefaultPbrMetallicValue);
    SetOptionalFloatUniform(programId, "u_RoughnessValue", kDefaultPbrRoughnessValue);
    SetOptionalIntUniform(programId, "u_HasAlbedoMap", 0);
}

} // namespace

// ---------------------------------------------------------------------------
//  Material
// ---------------------------------------------------------------------------

Material::Material(std::shared_ptr<ShaderProgram> shader)
    : m_shader(std::move(shader))
{
}

Material& Material::SetFloat(const std::string& name, float value) {
    m_floats[name] = value;
    return *this;
}

Material& Material::SetVec3(const std::string& name, glm::vec3 value) {
    m_vec3s[name] = value;
    return *this;
}

Material& Material::SetVec4(const std::string& name, glm::vec4 value) {
    m_vec4s[name] = value;
    return *this;
}

Material& Material::SetTexture(const std::string& slotName, std::shared_ptr<Texture2D> texture) {
    m_textures[slotName] = std::move(texture);
    return *this;
}

void Material::Bind() const {
    if (!m_shader) return;
    m_shader->Bind();
    ApplyPbrFallbackUniformDefaults(*m_shader);

    for (const auto& [name, val] : m_floats)  m_shader->SetUniform(name, val);
    for (const auto& [name, val] : m_vec3s)   m_shader->SetUniform(name, val);
    for (const auto& [name, val] : m_vec4s)   m_shader->SetUniform(name, val);

    GLuint unit = 0;
    for (const auto& [slotName, tex] : m_textures) {
        if (tex && tex->IsValid()) {
            tex->Bind(unit);
            m_shader->SetUniform(slotName, static_cast<int>(unit));
            if (slotName == TextureSlot::Albedo)
                SetOptionalIntUniform(m_shader->GetID(), "u_HasAlbedoMap", 1);
            ++unit;
        }
    }
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
    m_floats[name] = value;
    return *this;
}

MaterialInstance& MaterialInstance::SetVec3(const std::string& name, glm::vec3 value) {
    m_vec3s[name] = value;
    return *this;
}

MaterialInstance& MaterialInstance::SetVec4(const std::string& name, glm::vec4 value) {
    m_vec4s[name] = value;
    return *this;
}

MaterialInstance& MaterialInstance::SetTexture(const std::string& slotName,
                                                std::shared_ptr<Texture2D> texture) {
    m_textures[slotName] = std::move(texture);
    return *this;
}

void MaterialInstance::Bind() const {
    if (!m_parent) return;

    // Bind parent defaults first (shader, floats, vec3s, vec4s, parent textures).
    m_parent->Bind();

    // Get the shader from parent to apply instance overrides.
    ShaderProgram* shader = m_parent->GetShader();
    if (!shader) return;

    for (const auto& [name, val] : m_floats)  shader->SetUniform(name, val);
    for (const auto& [name, val] : m_vec3s)   shader->SetUniform(name, val);
    for (const auto& [name, val] : m_vec4s)   shader->SetUniform(name, val);

    // Instance textures override parent slots; assign units starting after parent's count.
    GLuint unit = static_cast<GLuint>(m_parent->m_textures.size());
    for (const auto& [slotName, tex] : m_textures) {
        if (tex && tex->IsValid()) {
            tex->Bind(unit);
            shader->SetUniform(slotName, static_cast<int>(unit));
            if (slotName == TextureSlot::Albedo)
                SetOptionalIntUniform(shader->GetID(), "u_HasAlbedoMap", 1);
            ++unit;
        }
    }
}

const ShaderProgram* MaterialInstance::GetShader() const {
    return m_parent ? m_parent->GetShader() : nullptr;
}

ShaderProgram* MaterialInstance::GetShader() {
    return m_parent ? m_parent->GetShader() : nullptr;
}
