#include "Buffer.h"
#include <glad/glad.h>
#include <spdlog/spdlog.h>

Buffer::Buffer(Type type, const void* data, GLsizeiptr size, GLenum usage) : m_type(type) {
    glGenBuffers(1, &m_id);
    if (m_id == 0) {
        const char* typeName = (type == VERTEX) ? "VBO" : (type == ELEMENT) ? "EBO" : "UBO";
        spdlog::error("[Buffer] glGenBuffers returned 0 — GL context may not be current or GPU buffer limit reached (type: {})", typeName);
        return;
    }

    Bind();
    GLenum target = (m_type == VERTEX) ? GL_ARRAY_BUFFER
                  : (m_type == ELEMENT) ? GL_ELEMENT_ARRAY_BUFFER
                  :                       GL_UNIFORM_BUFFER;
    glBufferData(target, size, data, usage);
    // Do not unbind EBOs: glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0) modifies the currently
    // bound VAO's state, which would silently detach this buffer from the VAO.
    // VERTEX and UNIFORM buffers are safe to unbind immediately.
    if (m_type != ELEMENT) Unbind();
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

static GLenum ToGLTarget(Buffer::Type type) {
    switch (type) {
        case Buffer::VERTEX:  return GL_ARRAY_BUFFER;
        case Buffer::ELEMENT: return GL_ELEMENT_ARRAY_BUFFER;
        case Buffer::UNIFORM: return GL_UNIFORM_BUFFER;
    }
    return GL_ARRAY_BUFFER;
}

void Buffer::Bind() const {
    glBindBuffer(ToGLTarget(m_type), m_id);
}

void Buffer::Unbind() const {
    glBindBuffer(ToGLTarget(m_type), 0);
}

void Buffer::UpdateData(const void* data, GLsizeiptr size, GLintptr offset) {
    Bind();
    glBufferSubData(ToGLTarget(m_type), offset, size, data);
    // EBO unbind while a VAO is bound detaches it from that VAO — skip for ELEMENT only.
    if (m_type != ELEMENT) Unbind();
}
