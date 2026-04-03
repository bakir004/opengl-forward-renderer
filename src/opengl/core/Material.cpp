#include "core/Material.h"
#include "core/ShaderProgram.h"
#include "core/Texture2D.h"
#include <spdlog/spdlog.h>

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

    for (const auto& [name, val] : m_floats)  m_shader->SetUniform(name, val);
    for (const auto& [name, val] : m_vec3s)   m_shader->SetUniform(name, val);
    for (const auto& [name, val] : m_vec4s)   m_shader->SetUniform(name, val);

    GLuint unit = 0;
    for (const auto& [slotName, tex] : m_textures) {
        if (tex && tex->IsValid()) {
            tex->Bind(unit);
            m_shader->SetUniform(slotName, static_cast<int>(unit));
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
