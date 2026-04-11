#include "core/ShaderProgram.h"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <unordered_set>

using std::string;

namespace {
    std::string LTrim(std::string s) {
        const size_t first = s.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        return s.substr(first);
    }

    std::optional<std::string> ReadFileWithIncludes(const std::filesystem::path& path,
                                                    std::unordered_set<std::string>& includeStack) {
        std::error_code ec;
        std::filesystem::path resolvedPath = std::filesystem::weakly_canonical(path, ec);
        if (ec) {
            resolvedPath = std::filesystem::absolute(path, ec);
            if (ec)
                resolvedPath = path;
        }

        const std::string key = resolvedPath.generic_string();
        if (includeStack.contains(key)) {
            spdlog::error("[Shader] Include cycle detected while loading '{}'", key);
            return std::nullopt;
        }

        std::ifstream file(resolvedPath);
        if (!file.is_open())
            return std::nullopt;

        includeStack.insert(key);

        std::ostringstream output;
        std::string line;
        while (std::getline(file, line)) {
            const std::string trimmed = LTrim(line);
            if (trimmed.rfind("#include", 0) == 0) {
                const size_t quoteStart = trimmed.find('"');
                const size_t quoteEnd = (quoteStart == std::string::npos)
                    ? std::string::npos
                    : trimmed.find('"', quoteStart + 1);

                if (quoteStart == std::string::npos || quoteEnd == std::string::npos || quoteEnd <= quoteStart + 1) {
                    spdlog::error("[Shader] Invalid include directive in '{}': {}", key, line);
                    includeStack.erase(key);
                    return std::nullopt;
                }

                const std::string includeName = trimmed.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                const std::filesystem::path includePath = resolvedPath.parent_path() / includeName;

                auto included = ReadFileWithIncludes(includePath, includeStack);
                if (!included) {
                    spdlog::error("[Shader] Failed to resolve include '{}' from '{}'", includeName, key);
                    includeStack.erase(key);
                    return std::nullopt;
                }

                output << *included << '\n';
            } else {
                output << line << '\n';
            }
        }

        includeStack.erase(key);
        return output.str();
    }
}

// === Hardcoded Fallback Strings ===
static const char* FALLBACK_VERT = R"(
    layout (location = 0) in vec3 aPos;
    void main() {
        gl_Position = vec4(aPos, 1.0);
    }
)";

static const char* FALLBACK_FRAG = R"(
    out vec4 FragColor;
    void main() {
        FragColor = vec4(1.0, 0.0, 1.0, 1.0);
    }
)";

ShaderProgram::ShaderProgram(const string& vertexPath, const string& fragmentPath)
{
    auto vertSource = ReadFile(vertexPath);
    auto fragSource = ReadFile(fragmentPath);

    GLuint vertShader = 0, fragShader = 0;

    if (vertSource && fragSource) {
        vertShader = CompileStage(*vertSource, GL_VERTEX_SHADER, vertexPath);
        if (vertShader != 0) {
            fragShader = CompileStage(*fragSource, GL_FRAGMENT_SHADER, fragmentPath);
            if (fragShader != 0) {
                m_id = LinkProgram(vertShader, fragShader, vertexPath, fragmentPath);
            }
        }
    } else {
        if (!vertSource) spdlog::error("[Shader] Could not read vertex shader file '{}'", vertexPath);
        if (!fragSource) spdlog::error("[Shader] Could not read fragment shader file '{}'", fragmentPath);
    }

    if (m_id == 0) {
        spdlog::warn("[Shader] '{}' + '{}' failed to compile/link — falling back to hardcoded magenta shader",
            vertexPath, fragmentPath);

        GLuint fbVert = CompileStage(FALLBACK_VERT, GL_VERTEX_SHADER,   "FALLBACK_VERT");
        GLuint fbFrag = CompileStage(FALLBACK_FRAG, GL_FRAGMENT_SHADER, "FALLBACK_FRAG");
        m_id = LinkProgram(fbVert, fbFrag, "FALLBACK_VERT", "FALLBACK_FRAG");
    } else {
        spdlog::info("[Shader] Linked: '{}' + '{}'", vertexPath, fragmentPath);
    }
}

ShaderProgram::~ShaderProgram(){
    if (m_id != 0) glDeleteProgram(m_id);
}

ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept
    : m_id(other.m_id), m_uniformCache(std::move(other.m_uniformCache)){
    other.m_id = 0;
}

ShaderProgram& ShaderProgram::operator=(ShaderProgram&& other) noexcept {
    if (this != &other){
        if (m_id != 0) glDeleteProgram(m_id);
        m_id = other.m_id;
        m_uniformCache = std::move(other.m_uniformCache);
        other.m_id = 0;
    }
    return *this;
}

void ShaderProgram::Bind() const { glUseProgram(m_id); }
void ShaderProgram::Unbind() { glUseProgram(0); }

void ShaderProgram::SetUniform(const string &name, float value) const {
    glUniform1f(GetUniformLocation(name), value);
}
void ShaderProgram::SetUniform(const string &name, int value) const {
    glUniform1i(GetUniformLocation(name), value);
}
void ShaderProgram::SetUniform(const string &name, bool value) const {
    glUniform1i(GetUniformLocation(name), static_cast<int>(value));
}
void ShaderProgram::SetUniform(const string &name, const glm::vec2 &value) const {
    glUniform2fv(GetUniformLocation(name), 1, glm::value_ptr(value));
}
void ShaderProgram::SetUniform(const string &name, const glm::vec3 &value) const {
    glUniform3fv(GetUniformLocation(name), 1, glm::value_ptr(value));
}
void ShaderProgram::SetUniform(const string &name, const glm::vec4 &value) const {
    glUniform4fv(GetUniformLocation(name), 1, glm::value_ptr(value));
}
void ShaderProgram::SetUniform(const string &name, const glm::mat3 &value) const {
    glUniformMatrix3fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}
void ShaderProgram::SetUniform(const string &name, const glm::mat4 &value) const {
    glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

std::optional<string> ShaderProgram::ReadFile(const string& path){
    std::unordered_set<std::string> includeStack;
    return ReadFileWithIncludes(std::filesystem::path(path), includeStack);
}

GLuint ShaderProgram::CompileStage(const string& source, GLenum stageType, const string& sourcePath){
    GLint major, minor;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    string versionStr = "#version " + std::to_string(major) + std::to_string(minor) + "0 core\n";
    const char* sources[] = { versionStr.c_str(), source.c_str() };

    GLuint shader = glCreateShader(stageType);
    glShaderSource(shader, 2, sources, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        string log(static_cast<size_t>(logLength), '\0');
        glGetShaderInfoLog(shader, logLength, nullptr, log.data());
        spdlog::error("[Shader] {} stage compile error in '{}':\n{}",
            (stageType == GL_VERTEX_SHADER ? "Vertex" : "Fragment"), sourcePath, log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint ShaderProgram::LinkProgram(GLuint vertShader, GLuint fragShader, const string& vertPath, const string& fragPath){
    GLuint program = glCreateProgram();
    glAttachShader(program, vertShader);
    glAttachShader(program, fragShader);
    glLinkProgram(program);

    glDetachShader(program, vertShader);
    glDetachShader(program, fragShader);
    glDeleteShader(vertShader);
    glDeleteShader(fragShader);

    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success){
        GLint logLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        string log(static_cast<size_t>(logLength), '\0');
        glGetProgramInfoLog(program, logLength, nullptr, log.data());
        spdlog::error("[Shader] Link error ('{}' + '{}'):\n{}", vertPath, fragPath, log);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

GLint ShaderProgram::GetUniformLocation(const string& name) const {
    auto it = m_uniformCache.find(name);
    if (it != m_uniformCache.end()) return it->second;

    GLint location = glGetUniformLocation(m_id, name.c_str());
    if (location == -1)
        spdlog::warn("[Shader] Uniform '{}' not found in program (ID {})", name, m_id);

    m_uniformCache[name] = location;
    return location;
}
