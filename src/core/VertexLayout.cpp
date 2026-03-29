#include "VertexLayout.h"

/// Returns the byte size of one GL type. Only types we actually use are listed;
/// add more here if you ever need GL_INT, GL_UNSIGNED_BYTE, etc.
static GLsizei GLTypeSize(GLenum type) {
    switch (type) {
        case GL_FLOAT:          return sizeof(GLfloat);
        case GL_UNSIGNED_INT:   return sizeof(GLuint);
        case GL_UNSIGNED_BYTE:  return sizeof(GLubyte);
        default:                return sizeof(GLfloat); // safe fallback
    }
}

GLsizei VertexAttribute::ByteSize() const {
    return count * GLTypeSize(type);
}

VertexLayout::VertexLayout(std::vector<VertexAttribute> attributes)
    : m_attributes(std::move(attributes))
{
    // Walk through every attribute and accumulate the total stride.
    // Stride = total byte size of one complete vertex.
    for (const auto& attrib : m_attributes) {
        m_stride += attrib.ByteSize();
    }
}

void VertexLayout::Apply() const {
    // We need to track how many bytes into the vertex each attribute starts.
    // First attribute starts at byte 0, second starts after the first, and so on.
    GLsizeiptr offset = 0;

    for (const auto& attrib : m_attributes) {
        // Tell OpenGL how to read this attribute out of the VBO:
        //   index      - which shader attribute slot (matches layout(location=N) in .glsl)
        //   count      - how many components (e.g. 3 for a vec3)
        //   type       - GL_FLOAT etc.
        //   normalized - whether to map int ranges to [0,1]
        //   stride     - byte distance between the start of one vertex and the next
        //   offset     - byte position of this attribute within one vertex
        glVertexAttribPointer(
            attrib.index,
            attrib.count,
            attrib.type,
            attrib.normalized,
            m_stride,
            reinterpret_cast<const void*>(offset)
        );

        // Attribute slots are disabled by default; we must enable each one.
        glEnableVertexAttribArray(attrib.index);

        // Advance the offset by how many bytes this attribute occupies.
        offset += attrib.ByteSize();
    }
}