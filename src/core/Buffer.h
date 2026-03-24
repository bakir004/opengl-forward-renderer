#pragma once
#include <glad/glad.h>

class Buffer {
public:
    enum Type { VERTEX, ELEMENT };

    Buffer(Type type, const void* data, GLuint size, GLenum usage);
    
    ~Buffer();

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;

    void Bind() const;
    void Unbind() const;

    void UpdateData(const void* data, GLuint size, GLintptr offset = 0);

private:
    GLuint m_id = 0;
    Type m_type;      // VERTEX or ELEMENT
};