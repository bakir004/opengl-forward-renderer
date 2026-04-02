#include "core/Renderer.h"
#include "core/MeshBuffer.h"
#include "core/ShaderProgram.h"
#include "core/UniformBuffer.h"
#include "scene/FrameSubmission.h"
#include "scene/RenderItem.h"
#include <glm/glm.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

#include <cassert>

#ifndef NDEBUG
/// GL debug message callback registered via glDebugMessageCallback (debug builds only).
/// Forwards high-severity messages as errors and medium-severity as warnings via spdlog.
static void APIENTRY gl_debug_callback(GLenum, GLenum, GLuint, GLenum severity, GLsizei, const char* message, const void*) {
    if (severity == GL_DEBUG_SEVERITY_HIGH)        spdlog::error("[GL] {}", message);
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
        spdlog::error("[Renderer] GLAD failed to load OpenGL function pointers — ensure a valid GL context is current before calling Initialize()");
        return false;
    }

    spdlog::info("[Renderer] Vendor:  {}", reinterpret_cast<const char*>(glGetString(GL_VENDOR)));
    spdlog::info("[Renderer] GPU:     {}", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
    spdlog::info("[Renderer] Version: {}", reinterpret_cast<const char*>(glGetString(GL_VERSION)));

#ifndef NDEBUG
    int flags = 0;
    glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
    if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(gl_debug_callback, nullptr);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
        spdlog::info("[Renderer] GL debug output enabled (synchronous)");
    }
#endif

    // Set the default pipeline state
    SetDepthTest(true, DepthFunc::Less);
    SetBlendMode(BlendMode::Disabled);
    SetCullMode(CullMode::Back);
    SetClearColor(m_clearColor);
    spdlog::info("[Renderer] Default pipeline state applied (depth=Less, blend=Disabled, cull=Back)");

    return true;
}


void Renderer::Shutdown() {
    spdlog::info("[Renderer] Shutdown complete");
}

void Renderer::BeginFrame(const FrameParams& params) {
    assert(!m_inFrame && "BeginFrame() called without a matching EndFrame()");
    m_inFrame = true;

    SetClearColor(params.clearColor);

    if (!!(params.clearFlags & ClearFlags::Depth))
        glDepthMask(GL_TRUE);

    Clear(params.clearFlags);
}

void Renderer::BeginFrame(const FrameSubmission& submission) {
    assert(!m_inFrame && "BeginFrame() called without a matching EndFrame()");
    m_inFrame = true;

    if (submission.camera) {
        if (!m_cameraUBO)
            m_cameraUBO = std::make_unique<UniformBuffer>(sizeof(CameraData), GL_DYNAMIC_DRAW);

        CameraData camData = submission.camera->BuildCameraData(submission.time, submission.deltaTime);
        m_cameraUBO->Upload(&camData, sizeof(CameraData));
        m_cameraUBO->BindToSlot(0);
    }

    SetViewport(submission.viewport);
    SetClearColor(submission.clearColor);

    if (!!(submission.clearFlags & ClearFlags::Depth))
        glDepthMask(GL_TRUE);

    Clear(submission.clearFlags);
}

void Renderer::EndFrame() {
    assert(m_inFrame && "EndFrame() called without a matching BeginFrame()");
    m_inFrame = false;
}

void Renderer::SetViewport(int x, int y, int width, int height) {
    Viewport requested{ x, y, width, height };
    if (m_viewport == requested) return;

    glViewport(x, y, width, height);
    m_viewport = requested;
    spdlog::debug("[Renderer] Viewport set to ({}, {}) {}x{}", x, y, width, height);
}

void Renderer::SetViewport(const Viewport& vp) {
    SetViewport(vp.x, vp.y, vp.width, vp.height);
}

void Renderer::SetClearColor(const glm::vec4& color) {
    if (color == m_clearColor) return;

    glClearColor(color.r, color.g, color.b, color.a);
    m_clearColor = color;
    spdlog::debug("[Renderer] Clear color: ({:.2f}, {:.2f}, {:.2f}, {:.2f})",
        color.r, color.g, color.b, color.a);
}

void Renderer::Clear(ClearFlags flags) {
    if (!flags) return;

    const GLbitfield mask = ToGLClearMask(flags);
    glClear(mask);
}

void Renderer::Resize(int width, int height) {
    SetViewport(0, 0, width, height);
}

void Renderer::SetDepthTest(const bool enable, const DepthFunc func) {
    if (m_depthTestEnabled != enable) {
        if (enable) {
            glEnable(GL_DEPTH_TEST);
            spdlog::debug("[Renderer] Depth test enabled");
        } else {
            glDisable(GL_DEPTH_TEST);
            spdlog::debug("[Renderer] Depth test disabled");
        }
        m_depthTestEnabled = enable;
    }

    if (enable && m_depthFunc != func) {
        GLenum glFunc = 0;
        const char* funcName;
        switch (func) {
            case DepthFunc::Less:           glFunc = GL_LESS;      funcName = "Less";         break;
            case DepthFunc::LessEqual:      glFunc = GL_LEQUAL;    funcName = "LessEqual";    break;
            case DepthFunc::Greater:        glFunc = GL_GREATER;   funcName = "Greater";      break;
            case DepthFunc::GreaterEqual:   glFunc = GL_GEQUAL;    funcName = "GreaterEqual"; break;
            case DepthFunc::Always:         glFunc = GL_ALWAYS;    funcName = "Always";       break;
            case DepthFunc::Never:          glFunc = GL_NEVER;     funcName = "Never";        break;
            case DepthFunc::Equal:          glFunc = GL_EQUAL;     funcName = "Equal";        break;
            case DepthFunc::NotEqual:       glFunc = GL_NOTEQUAL;  funcName = "NotEqual";     break;
        }
        glDepthFunc(glFunc);
        m_depthFunc = func;
        spdlog::debug("[Renderer] Depth func: {}", funcName);
    }
}

void Renderer::SetBlendMode(const BlendMode mode) {
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
    spdlog::debug("[Renderer] Blend mode: {}", modeName);
}

void Renderer::SetCullMode(const CullMode mode) {
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
    spdlog::debug("[Renderer] Cull mode: {}", modeName);
}

void Renderer::SubmitDraw(const ShaderProgram& shader, const MeshBuffer& mesh) {
    SubmitDraw(shader, mesh, glm::mat4(1.0f));
}

void Renderer::SubmitDraw(const ShaderProgram& shader, const MeshBuffer& mesh, const glm::mat4& modelMatrix) {
    assert(m_inFrame && "SubmitDraw() must be called between BeginFrame() and EndFrame()");

    shader.Bind();
    shader.SetUniform("u_Model", modelMatrix);

    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(modelMatrix)));
    shader.SetUniform("u_NormalMatrix", normalMatrix);

    mesh.Draw();
}

void Renderer::SubmitDraw(const RenderItem& item) {
    if (!item.flags.visible || !item.mesh || !item.shader)
        return;

    // Set render mode.
    switch (item.drawMode) {
        case DrawMode::Fill:      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);   break;
        case DrawMode::Wireframe: glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);   break;
        case DrawMode::Points:    glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);  break;
    }

    // Topology is handled by MeshBuffer::Draw (for now assumes triangle).  
    // If needed, MeshBuffer can expose draw call variant to honor topology.

    SubmitDraw(*item.shader, *item.mesh, item.transform.GetModelMatrix());

    // Restore fill mode by default (to avoid affecting subsequent draws).
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void Renderer::UnbindShader() {
    ShaderProgram::Unbind();
}
