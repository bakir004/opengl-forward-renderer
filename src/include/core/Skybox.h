#pragma once

#include <memory>
#include <vector>
#include <string>
#include <glm/glm.hpp>

class MeshBuffer;
class TextureCubemap;
class ShaderProgram;

/**
 * @class Skybox
 * @brief Manages the rendering of a cubemap skybox.
 */
class Skybox {
public:
    /**
     * @brief Creates a skybox from 6 face textures.
     * @param facePaths Paths to the 6 face images (+X, -X, +Y, -Y, +Z, -Z).
     */
    explicit Skybox(const std::vector<std::string>& facePaths);

    /**
     * @brief Creates a skybox using an existing cubemap texture.
     */
    explicit Skybox(std::shared_ptr<TextureCubemap> cubemap);

    ~Skybox();

    /**
     * @brief Renders the skybox.
     * @param projection The projection matrix.
     * @param view The view matrix (will be stripped of translation internally).
     */
    void Draw(const glm::mat4& projection, const glm::mat4& view) const;

    [[nodiscard]] std::shared_ptr<TextureCubemap> GetTexture() const { return m_cubemap; }

    void SetTint(const glm::vec3& tint)            { m_tint = tint; }
    void SetExposure(float exposure)               { m_exposure = exposure; }
    void SetEmissiveColor(const glm::vec3& color)  { m_emissiveColor = color; }
    void SetEmissiveStrength(float strength)       { m_emissiveStrength = strength; }

    [[nodiscard]] glm::vec3 GetTint()             const { return m_tint; }
    [[nodiscard]] float     GetExposure()         const { return m_exposure; }
    [[nodiscard]] glm::vec3 GetEmissiveColor()    const { return m_emissiveColor; }
    [[nodiscard]] float     GetEmissiveStrength() const { return m_emissiveStrength; }

private:
    void Init();

    std::shared_ptr<MeshBuffer> m_cubeMesh;
    std::shared_ptr<TextureCubemap> m_cubemap;
    std::shared_ptr<ShaderProgram> m_shader;

    glm::vec3 m_tint            = {1.0f, 1.0f, 1.0f};
    float     m_exposure        = 1.0f;
    glm::vec3 m_emissiveColor   = {0.0f, 0.0f, 0.0f};
    float     m_emissiveStrength = 0.0f;
};
