#pragma once
#include <glad/glad.h>

class Buffer {
public:
    enum Type { VERTEX, ELEMENT };

    // GLsizeiptr instead of GLuint: matches the GL API signature and avoids truncation on 64-bit for large buffers
    Buffer(Type type, const void* data, GLsizeiptr size, GLenum usage);
    
    ~Buffer();

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;

    void Bind() const;
    void Unbind() const;

    // GLsizeiptr instead of GLuint: matches the GL API signature and avoids truncation on 64-bit for large buffers
    void UpdateData(const void* data, GLsizeiptr size, GLintptr offset = 0);

private:
    GLuint m_id = 0;
    Type m_type;      // VERTEX or ELEMENT
};
