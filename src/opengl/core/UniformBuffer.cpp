#include "core/UniformBuffer.h"
#include <spdlog/spdlog.h>

UniformBuffer::UniformBuffer(GLsizeiptr size, GLenum usage)
    : m_buffer(Buffer::UNIFORM, nullptr, size, usage)
{
    spdlog::debug("[UniformBuffer] Allocated {} bytes", size);
}

void UniformBuffer::Upload(const void* data, GLsizeiptr size, GLintptr offset)
{
    m_buffer.UpdateData(data, size, offset);
}

void UniformBuffer::BindToSlot(GLuint bindingPoint) const
{
    glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, m_buffer.GetID());
}
