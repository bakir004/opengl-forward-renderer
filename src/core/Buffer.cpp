#include "Buffer.h"
#include <glad/glad.h>
#include <spdlog/spdlog.h>

Buffer::Buffer(Type type, const void* data, GLsizeiptr size, GLenum usage) : m_type(type) {
    glGenBuffers(1, &m_id);
    if (m_id == 0) {
        spdlog::error("Failed to generate OpenGL buffer");
        return;
    }

    Bind();
    GLenum target = (m_type == VERTEX) ? GL_ARRAY_BUFFER : GL_ELEMENT_ARRAY_BUFFER;
    glBufferData(target, size, data, usage);
    // Do not unbind EBOs: glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0) modifies the currently
    // bound VAO's state, which would silently detach this buffer from the VAO.
    if (m_type == VERTEX) Unbind();
}

Buffer::~Buffer() {
    if (m_id != 0) {
        glDeleteBuffers(1, &m_id);
        m_id = 0;
    }
}

Buffer::Buffer(Buffer&& other) noexcept : m_id(other.m_id), m_type(other.m_type) {
    other.m_id = 0;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
        if (m_id != 0) glDeleteBuffers(1, &m_id);
        m_id = other.m_id;
        m_type = other.m_type;
        other.m_id = 0;
    }
    return *this;
}

void Buffer::Bind() const {
    GLenum target = (m_type == VERTEX) ? GL_ARRAY_BUFFER : GL_ELEMENT_ARRAY_BUFFER;
    glBindBuffer(target, m_id);
}

void Buffer::Unbind() const {
    GLenum target = (m_type == VERTEX) ? GL_ARRAY_BUFFER : GL_ELEMENT_ARRAY_BUFFER;
    glBindBuffer(target, 0);
}

void Buffer::UpdateData(const void* data, GLsizeiptr size, GLintptr offset) {
    Bind();
    GLenum target = (m_type == VERTEX) ? GL_ARRAY_BUFFER : GL_ELEMENT_ARRAY_BUFFER;
    glBufferSubData(target, offset, size, data);
    // Same reason as constructor: unbinding an EBO while a VAO is bound detaches it from that VAO.
    if (m_type == VERTEX) Unbind();
}
