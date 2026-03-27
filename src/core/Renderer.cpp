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

    // Set default pipeline state
    SetDepthTest(true, DepthFunc::Less);
    SetBlendMode(BlendMode::Disabled);
    SetCullMode(CullMode::Back);
    spdlog::info("Pipeline state initialized");

    return true;
}

void Renderer::Resize(int w, int h){ glViewport(0, 0, w, h); }
void Renderer::RenderFrame(){ glClearColor(0.1f, 0.1f, 0.1f, 1.0f); glClear(GL_COLOR_BUFFER_BIT); }
void Renderer::Shutdown(){ spdlog::info("Renderer shutdown"); }

void Renderer::SetDepthTest(bool enable, DepthFunc func) {

    if (m_depthTestEnabled != enable) {
        if (enable) {
            glEnable(GL_DEPTH_TEST);
        } else {
            glDisable(GL_DEPTH_TEST);
        }
        m_depthTestEnabled = enable;
    }

    // Only update if state actually changed
    if (enable && m_depthFunc != func) {
        GLenum glFunc;
        switch (func) {
            case DepthFunc::Less:           glFunc = GL_LESS; break;
            case DepthFunc::LessEqual:      glFunc = GL_LEQUAL; break;
            case DepthFunc::Greater:        glFunc = GL_GREATER; break;
            case DepthFunc::GreaterEqual:   glFunc = GL_GEQUAL; break;
            case DepthFunc::Always:         glFunc = GL_ALWAYS; break;
            case DepthFunc::Never:          glFunc = GL_NEVER; break;
            case DepthFunc::Equal:          glFunc = GL_EQUAL; break;
            case DepthFunc::NotEqual:       glFunc = GL_NOTEQUAL; break;
        }
        glDepthFunc(glFunc);
        m_depthFunc = func;
    }
}

void Renderer::SetBlendMode(BlendMode mode) {

    // Only update if state actually changed
    if (m_blendMode == mode) return;

    switch (mode) {
        case BlendMode::Disabled:
            glDisable(GL_BLEND);
            break;

        case BlendMode::Alpha:
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glBlendEquation(GL_FUNC_ADD);
            break;

        case BlendMode::Additive:
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glBlendEquation(GL_FUNC_ADD);
            break;

        case BlendMode::Multiply:
            glEnable(GL_BLEND);
            glBlendFunc(GL_DST_COLOR, GL_ZERO);
            glBlendEquation(GL_FUNC_ADD);
            break;
    }

    m_blendMode = mode;
}

void Renderer::SetCullMode(CullMode mode) {

    // Only update if state actually changed
    if (m_cullMode == mode) return;

    switch (mode) {
        case CullMode::Disabled:
            glDisable(GL_CULL_FACE);
            break;

        case CullMode::Back:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            break;

        case CullMode::Front:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            break;

        case CullMode::FrontAndBack:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT_AND_BACK);
            break;
    }

    m_cullMode = mode;
}