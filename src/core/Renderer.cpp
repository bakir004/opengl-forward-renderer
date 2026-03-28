#include "Renderer.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

#include <cassert>

#ifndef NDEBUG
/// GL debug message callback registered via glDebugMessageCallback (debug builds only).
/// Forwards high-severity messages as errors and medium-severity as warnings via spdlog.
static void APIENTRY gl_debug_callback(GLenum, GLenum, GLuint, GLenum severity, GLsizei, const char* message, const void*) {
    if (severity == GL_DEBUG_SEVERITY_HIGH)   spdlog::error("[GL] {}", message);
    else if (severity == GL_DEBUG_SEVERITY_MEDIUM) spdlog::warn("[GL] {}", message);
}
#endif
/// Maps our ClearFlags bitmask to the GLbitfield that glClear expects.
static GLbitfield ToGLClearMask(ClearFlags flags) {
    GLbitfield mask = 0;
    if (!!(flags & ClearFlags::Color))   mask |= GL_COLOR_BUFFER_BIT;
    if (!!(flags & ClearFlags::Depth))   mask |= GL_DEPTH_BUFFER_BIT;
    if (!!(flags & ClearFlags::Stencil)) mask |= GL_STENCIL_BUFFER_BIT;
    return mask;
}


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
    SetClearColor(m_clearColor);          // push cached default to GL
    spdlog::info("Pipeline state initialized");

    return true;
}


void Renderer::Shutdown(){ spdlog::info("Renderer shutdown"); }

void Renderer::BeginFrame(const FrameParams& params) {
    // In debug builds, catch BeginFrame called twice without EndFrame.
    assert(!m_inFrame && "BeginFrame() called without a matching EndFrame()");
    m_inFrame = true;

    // Apply this frame's clear colour (cached - no GL call if unchanged).
    SetClearColor(params.clearColor);

    // Clear whichever buffers were requested.
    if (!!(params.clearFlags & ClearFlags::Depth)) {
        // Make sure depth writes are enabled so the depth buffer actually clears.
        // If someone called glDepthMask(GL_FALSE) earlier, the clear would be a no-op.
        glDepthMask(GL_TRUE);
    }
    Clear(params.clearFlags);

    // Sprint 3 will push the view + projection UBO here.
}

void Renderer::EndFrame() {
    assert(m_inFrame && "EndFrame() called without a matching BeginFrame()");
    m_inFrame = false;

    // Nothing to do yet.
    // Sprint 3+: GPU fence, profiler scope close, per-frame stat reset.
}

void Renderer::SetViewport(int x, int y, int width, int height) {
    Viewport requested{ x, y, width, height };
    if (m_viewport == requested) return; // skip redundant driver call

    glViewport(x, y, width, height);
    m_viewport = requested;
    spdlog::debug("Viewport: ({}, {})  {}x{}", x, y, width, height);
}

void Renderer::SetViewport(const Viewport& vp) {
    SetViewport(vp.x, vp.y, vp.width, vp.height);
}

void Renderer::SetClearColor(const glm::vec4& color) {
    // glm::vec4 has no operator== so we compare component-wise.
    if (color == m_clearColor) return; // cached

    glClearColor(color.r, color.g, color.b, color.a);
    m_clearColor = color;
    spdlog::debug("Clear colour: ({:.2f}, {:.2f}, {:.2f}, {:.2f})",
        color.r, color.g, color.b, color.a);
}

void Renderer::Clear(ClearFlags flags) {
    if (!flags) return; // ClearFlags::None - nothing to do

    const GLbitfield mask = ToGLClearMask(flags);
    glClear(mask);
}
void Renderer::Resize(int width, int height) {
    // Funnels through SetViewport so the cache stays consistent and we log
    // the change in one place.
    SetViewport(0, 0, width, height);
}
void Renderer::RenderFrame() {
    BeginFrame();
    // No geometry yet - Sprint 3 will submit the scene here.
    EndFrame();
}

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