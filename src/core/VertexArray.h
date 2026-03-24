#pragma once
#include <glad/glad.h>

class VertexArray {
public:
    VertexArray();
    ~VertexArray();

    VertexArray(const VertexArray&) = delete;
    VertexArray& operator=(const VertexArray&) = delete;

    VertexArray(VertexArray&& other) noexcept;
    VertexArray& operator=(VertexArray&& other) noexcept;

    void Bind() const;
    void Unbind() const;

private:
    GLuint m_id = 0;
};