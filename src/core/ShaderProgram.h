#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <optional>
#include <unordered_map>

/// @brief Manages the lifecycle and uniform state of an OpenGL shader program.
class ShaderProgram
{
private:
    static std::optional<std::string> ReadFile(const std::string& path);
    static GLuint CompileStage(const std::string& source, GLenum stageType, const std::string& sourcePath);
    static GLuint LinkProgram(GLuint vertShader, GLuint fragShader, const std::string& vertPath, const std::string& fragPath);
    GLint GetUniformLocation(const std::string& name) const;

    GLuint m_id = 0;
    mutable std::unordered_map<std::string, GLint> m_uniformCache;
    
public:
    /// @brief Loads, compiles, and links a shader program from disk.
    /// @param vertexPath Path to .vert file.
    /// @param fragmentPath Path to .frag file.
    ShaderProgram(const std::string& vertexPath, const std::string& fragmentPath);
    ~ShaderProgram();

    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;

    /// @brief Move constructor: transfers GL program ownership.
    ShaderProgram(ShaderProgram&& other) noexcept;
    ShaderProgram& operator=(ShaderProgram&& other) noexcept;

    /// @brief Checks if the program was successfully compiled and linked.
    [[nodiscard]] bool IsValid() const { return m_id != 0; }

    /// @brief Returns the raw OpenGL handle.
    [[nodiscard]] GLuint GetID() const { return m_id; }

    /// @brief Activates the shader program for rendering.
    void Bind() const;

    /// @brief Disables the current shader program.
    static void Unbind();

    /// @name Uniform Setters
    /// @{
    void SetUniform(const std::string& name, float value) const;
    void SetUniform(const std::string& name, int value) const;
    void SetUniform(const std::string& name, bool value) const;
    void SetUniform(const std::string& name, const glm::vec2& value) const;
    void SetUniform(const std::string& name, const glm::vec3& value) const;
    void SetUniform(const std::string& name, const glm::vec4& value) const;
    void SetUniform(const std::string& name, const glm::mat3& value) const;
    void SetUniform(const std::string& name, const glm::mat4& value) const;
    /// @}
};