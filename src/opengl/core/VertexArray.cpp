#include "core/VertexArray.h"
#include <spdlog/spdlog.h>
#include <glad/glad.h>

VertexArray::VertexArray() {
    glGenVertexArrays(1, &m_id);
    if (m_id == 0) {
        spdlog::error("[VAO] glGenVertexArrays returned 0 — GL context may not be current or GPU VAO limit reached");
    }
}

VertexArray::~VertexArray() {
    if (m_id != 0) {
        glDeleteVertexArrays(1, &m_id);
        m_id = 0;
    }
}

VertexArray::VertexArray(VertexArray&& other) noexcept : m_id(other.m_id) {
    other.m_id = 0;
}

VertexArray& VertexArray::operator=(VertexArray&& other) noexcept {
    if (this != &other) {
        if (m_id != 0) glDeleteVertexArrays(1, &m_id);
        m_id = other.m_id;
        other.m_id = 0;
    }
    return *this;
}

void VertexArray::Bind() const {
    glBindVertexArray(m_id);
}

void VertexArray::Unbind() const {
    glBindVertexArray(0);
}
