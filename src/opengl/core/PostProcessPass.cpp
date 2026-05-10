#include "core/PostProcessPass.h"
#include <glad/glad.h>

void PostProcessPass::SetupViewport(uint32_t width, uint32_t height)
{
    glGetIntegerv(GL_VIEWPORT, reinterpret_cast<GLint *>(m_savedViewport));
    glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
}

void PostProcessPass::RestoreViewport()
{
    glViewport(m_savedViewport[0], m_savedViewport[1],
               m_savedViewport[2], m_savedViewport[3]);
}
