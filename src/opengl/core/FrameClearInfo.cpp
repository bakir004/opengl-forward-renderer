#include "core/FrameClearInfo.h"
#include <glad/glad.h>

static GLbitfield ToGLClearMask(ClearFlags flags) {
    GLbitfield mask = 0;
    if (!!(flags & ClearFlags::Color))   mask |= GL_COLOR_BUFFER_BIT;
    if (!!(flags & ClearFlags::Depth))   mask |= GL_DEPTH_BUFFER_BIT;
    if (!!(flags & ClearFlags::Stencil)) mask |= GL_STENCIL_BUFFER_BIT;
    return mask;
}

void FrameClearInfo::Apply() const {
    glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
    glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);

    // Ensure depth buffer is writable before clearing it.
    if (!!(clearFlags & ClearFlags::Depth))
        glDepthMask(GL_TRUE);

    const GLbitfield mask = ToGLClearMask(clearFlags);
    if (mask)
        glClear(mask);
}
