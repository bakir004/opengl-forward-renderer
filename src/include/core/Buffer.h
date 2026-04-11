#pragma once
#include <glad/glad.h>

/// RAII wrapper around an OpenGL buffer object (VBO, EBO, or UBO).
/// Manages GPU memory lifetime — the buffer is deleted when this object is destroyed.
/// Copy is disabled; use move semantics to transfer ownership.
class Buffer {
public:
    enum Type { VERTEX, ELEMENT, UNIFORM };

    /// Allocates a GPU buffer and uploads initial data.
    /// @param type   VERTEX for GL_ARRAY_BUFFER, ELEMENT for GL_ELEMENT_ARRAY_BUFFER
    /// @param data   Pointer to the data to upload (may be nullptr to allocate without initializing)
    /// @param size   Size in bytes — GLsizeiptr to match the GL API and avoid truncation on 64-bit
    /// @param usage  GL usage hint (e.g. GL_STATIC_DRAW, GL_DYNAMIC_DRAW)
    Buffer(Type type, const void* data, GLsizeiptr size, GLenum usage);

    /// Deletes the underlying GL buffer object.
    ~Buffer();

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    /// Transfers ownership of the GL buffer; leaves other in a null (non-owning) state.
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;

    /// Binds this buffer to its target (GL_ARRAY_BUFFER or GL_ELEMENT_ARRAY_BUFFER).
    void Bind() const;

    /// Unbinds this buffer's target.
    /// Note: do not call while a VAO is bound if this is an ELEMENT buffer —
    /// unbinding GL_ELEMENT_ARRAY_BUFFER modifies the active VAO's EBO binding.
    void Unbind() const;

    /// Partially or fully updates the buffer's GPU data via glBufferSubData.
    /// @param data   Pointer to the new data
    /// @param size   Number of bytes to write — GLsizeiptr to match the GL API
    /// @param offset Byte offset into the buffer to start writing at (default 0)
    void UpdateData(const void* data, GLsizeiptr size, GLintptr offset = 0);

    /// Returns the underlying GL buffer object ID.
    /// Used by higher-level wrappers (e.g. UniformBuffer) that need to call GL functions
    /// which require the raw ID and have no equivalent on Buffer itself (e.g. glBindBufferBase).
    GLuint GetID() const { return m_id; }

private:
    GLuint m_id = 0;
    Type m_type;      // VERTEX, ELEMENT, or UNIFORM
};
