#pragma once
#include <glad/glad.h>

/// RAII wrapper around an OpenGL Vertex Array Object (VAO).
/// Encapsulates vertex attribute state and EBO binding.
/// Copy is disabled; use move semantics to transfer ownership.
class VertexArray {
public:
    /// Generates a new VAO.
    VertexArray();

    /// Deletes the underlying GL vertex array object.
    ~VertexArray();

    VertexArray(const VertexArray&) = delete;
    VertexArray& operator=(const VertexArray&) = delete;

    /// Transfers ownership of the GL VAO; leaves other in a null (non-owning) state.
    VertexArray(VertexArray&& other) noexcept;
    VertexArray& operator=(VertexArray&& other) noexcept;

    /// Binds this VAO, making it the active vertex array for subsequent draw calls.
    void Bind() const;

    /// Unbinds the currently bound VAO.
    void Unbind() const;

private:
    GLuint m_id = 0;
};
