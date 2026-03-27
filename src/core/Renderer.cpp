#include "Renderer.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

#ifndef NDEBUG
/// GL debug message callback registered via glDebugMessageCallback (debug builds only).
/// Forwards high-severity messages as errors and medium-severity as warnings via spdlog.
static void APIENTRY gl_debug_callback(GLenum, GLenum, GLuint, GLenum severity, GLsizei, const char* message, const void*) {
    if (severity == GL_DEBUG_SEVERITY_HIGH)   spdlog::error("[GL] {}", message);
    else if (severity == GL_DEBUG_SEVERITY_MEDIUM) spdlog::warn("[GL] {}", message);
}
#endif

bool Renderer::Initialize() {
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        spdlog::error("GLAD initialization failed");
        return false;
    }

    spdlog::info("Vendor:  {}", reinterpret_cast<const char*>(glGetString(GL_VENDOR)));
    spdlog::info("GPU:     {}", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
    spdlog::info("Version: {}", reinterpret_cast<const char*>(glGetString(GL_VERSION)));

#ifndef NDEBUG
    int flags = 0;
    glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
    if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(gl_debug_callback, nullptr);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
        spdlog::info("GL debug context active");
    }
#endif

    // Set the default pipeline state
    SetDepthTest(true, DepthFunc::Less);
    SetBlendMode(BlendMode::Disabled);
    SetCullMode(CullMode::Back);
    spdlog::info("Pipeline state initialized");

    return true;
}

void Renderer::Resize(int w, int h){ glViewport(0, 0, w, h); }
void Renderer::RenderFrame(){ glClearColor(0.1f, 0.1f, 0.1f, 1.0f); glClear(GL_COLOR_BUFFER_BIT); }
void Renderer::Shutdown(){ spdlog::info("Renderer shutdown"); }

void Renderer::SetDepthTest(const bool enable, const DepthFunc func) {
    // Only update if the state actually changed
    if (m_depthTestEnabled != enable) {
        if (enable) {
            glEnable(GL_DEPTH_TEST);
            spdlog::debug("Depth test enabled");
        } else {
            glDisable(GL_DEPTH_TEST);
            spdlog::debug("Depth test disabled");
        }
        m_depthTestEnabled = enable;
    }

    if (enable && m_depthFunc != func) {
        GLenum glFunc = 0;
        const char* funcName;
        switch (func) {
            case DepthFunc::Less:           glFunc = GL_LESS;       funcName = "Less"; break;
            case DepthFunc::LessEqual:      glFunc = GL_LEQUAL;     funcName = "LessEqual"; break;
            case DepthFunc::Greater:        glFunc = GL_GREATER;    funcName = "Greater"; break;
            case DepthFunc::GreaterEqual:   glFunc = GL_GEQUAL;     funcName = "GreaterEqual"; break;
            case DepthFunc::Always:         glFunc = GL_ALWAYS;     funcName = "Always"; break;
            case DepthFunc::Never:          glFunc = GL_NEVER;      funcName = "Never"; break;
            case DepthFunc::Equal:          glFunc = GL_EQUAL;      funcName = "Equal"; break;
            case DepthFunc::NotEqual:       glFunc = GL_NOTEQUAL;   funcName = "NotEqual"; break;
        }
        glDepthFunc(glFunc);
        m_depthFunc = func;
        spdlog::debug("Depth func: {}", funcName);
    }
}

void Renderer::SetBlendMode(const BlendMode mode) {
    // Only update if the state actually changed
    if (m_blendMode == mode) return;

    const char* modeName;
    switch (mode) {
        case BlendMode::Disabled:
            glDisable(GL_BLEND);
            modeName = "Disabled";
            break;

        case BlendMode::Alpha:
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glBlendEquation(GL_FUNC_ADD);
            modeName = "Alpha";
            break;

        case BlendMode::Additive:
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glBlendEquation(GL_FUNC_ADD);
            modeName = "Additive";
            break;

        case BlendMode::Multiply:
            glEnable(GL_BLEND);
            glBlendFunc(GL_DST_COLOR, GL_ZERO);
            glBlendEquation(GL_FUNC_ADD);
            modeName = "Multiply";
            break;
    }

    m_blendMode = mode;
    spdlog::debug("Blend mode: {}", modeName);
}

void Renderer::SetCullMode(const CullMode mode) {
    // Only update if the state actually changed
    if (m_cullMode == mode) return;

    const char* modeName;
    switch (mode) {
        case CullMode::Disabled:
            glDisable(GL_CULL_FACE);
            modeName = "Disabled";
            break;

        case CullMode::Back:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            modeName = "Back";
            break;

        case CullMode::Front:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            modeName = "Front";
            break;

        case CullMode::FrontAndBack:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT_AND_BACK);
            modeName = "FrontAndBack";
            break;
    }

    m_cullMode = mode;
    spdlog::debug("Cull mode: {}", modeName);
}