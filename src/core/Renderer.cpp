#include "Renderer.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

#ifndef NDEBUG
static void APIENTRY gl_debug_callback(GLenum, GLenum, GLuint, GLenum severity, GLsizei, const char* message, const void*) {
    if (severity == GL_DEBUG_SEVERITY_HIGH)   spdlog::error("[GL] {}", message);
    else if (severity == GL_DEBUG_SEVERITY_MEDIUM) spdlog::warn("[GL] {}", message);
}
#endif

bool Renderer::Initialize(const nlohmann::json&) {
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

    return true;
}

void Renderer::Resize(int w, int h){ glViewport(0, 0, w, h); }
void Renderer::RenderFrame(){ glClearColor(0.1f, 0.1f, 0.1f, 1.0f); glClear(GL_COLOR_BUFFER_BIT); }
void Renderer::Shutdown(){ spdlog::info("Renderer shutdown"); }