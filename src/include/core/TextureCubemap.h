#pragma once

#include <glad/glad.h>
#include <string>
#include <vector>
#include <array>

enum class TextureColorSpace;

/**
 * @class TextureCubemap
 * @brief RAII owner of an OpenGL Cube Map texture.
 * 
 * Loads 6 images representing the faces of a cube:
 * +X (Right), -X (Left), +Y (Top), -Y (Bottom), +Z (Front), -Z (Back)
 */
class TextureCubemap {
public:
    /**
     * @brief Loads 6 face images and creates a cubemap.
     * @param facePaths Array of 6 paths in order: +X, -X, +Y, -Y, +Z, -Z.
     * @param colorSpace sRGB for color data, Linear for non-color data.
     * @param flipY Whether to flip images vertically on load.
     */
    explicit TextureCubemap(const std::array<std::string, 6>& facePaths,
                            TextureColorSpace colorSpace,
                            bool flipY = false);

    ~TextureCubemap();

    TextureCubemap(const TextureCubemap&) = delete;
    TextureCubemap& operator=(const TextureCubemap&) = delete;

    TextureCubemap(TextureCubemap&& other) noexcept;
    TextureCubemap& operator=(TextureCubemap&& other) noexcept;

    void Bind(GLuint unit = 0) const;
    static void Unbind(GLuint unit = 0);

    [[nodiscard]] GLuint GetID() const { return m_id; }
    [[nodiscard]] bool IsValid() const { return m_id != 0; }

private:
    TextureCubemap() = default;

    GLuint m_id = 0;
    int m_width = 0;
    int m_height = 0;
};
