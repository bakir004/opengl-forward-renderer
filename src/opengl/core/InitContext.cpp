#include "core/InitContext.h"
#include "core/SubmissionContext.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

#ifndef NDEBUG
static void APIENTRY gl_debug_callback(GLenum, GLenum, GLuint, GLenum severity,
                                        GLsizei, const char* message, const void*)
{
    if (severity == GL_DEBUG_SEVERITY_HIGH)        spdlog::error("[GL] {}", message);
    else if (severity == GL_DEBUG_SEVERITY_MEDIUM) spdlog::warn("[GL] {}", message);
}
#endif

bool InitContext::Initialize(SubmissionContext& outApplied) {
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        spdlog::error("[InitContext] GLAD failed to load OpenGL function pointers — "
                      "ensure a valid GL context is current");
        return false;
    }

    spdlog::info("[InitContext] Vendor:  {}", reinterpret_cast<const char*>(glGetString(GL_VENDOR)));
    spdlog::info("[InitContext] GPU:     {}", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
    spdlog::info("[InitContext] Version: {}", reinterpret_cast<const char*>(glGetString(GL_VERSION)));

#ifndef NDEBUG
    int flags = 0;
    glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
    if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(gl_debug_callback, nullptr);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
        spdlog::info("[InitContext] GL debug output enabled (synchronous)");
    }
#endif

    // Force all fields to differ from the defaults so every GL call fires on first Apply.
    SubmissionContext current;
    current.depthTestEnabled = false;
    current.depthFunc        = DepthFunc::Always;
    current.depthWrite       = false;
    current.blendMode        = BlendMode::Alpha;
    current.cullMode         = CullMode::Disabled;

    SubmissionContext defaults{};  // depthTest=true, Less, depthWrite=true, Disabled, Back
    defaults.Apply(current);       // pushes every field to the GPU; current now mirrors defaults
    outApplied = current;

    spdlog::info("[InitContext] Default pipeline state applied (depth=Less, blend=Disabled, cull=Back)");
    return true;
}

void InitContext::Shutdown() {
    spdlog::info("[InitContext] Shutdown complete");
}
