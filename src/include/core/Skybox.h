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

private:
    void Init();

    std::shared_ptr<MeshBuffer> m_cubeMesh;
    std::shared_ptr<TextureCubemap> m_cubemap;
    std::shared_ptr<ShaderProgram> m_shader;
};
