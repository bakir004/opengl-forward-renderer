#pragma once
#include <glad/glad.h>
#include <vector>

/// Describes a single vertex attribute (e.g. position, color, UV).
/// Used internally by VertexLayout to calculate stride and offsets automatically.
struct VertexAttribute {
    GLuint     index;       ///< Attribute location in the shader (layout(location = N))
    GLint      count;       ///< Number of components: 2 (UV), 3 (XYZ/RGB), 4 (RGBA)
    GLenum     type;        ///< GL data type, almost always GL_FLOAT
    GLboolean  normalized;  ///< Whether integer types should be normalized to [0,1]

    /// Returns the size in bytes of this attribute (e.g. 3 floats = 12 bytes).
    GLsizei ByteSize() const;
};

/// Describes the memory layout of one vertex: what attributes it has and in what order.
///
/// Usage pattern:
///   1. Construct with a list of VertexAttributes
///   2. Bind your VAO and VBO
///   3. Call Apply() — it sets up all glVertexAttribPointer calls automatically
///
/// Example — position (location 0) + color (location 1):
///   VertexLayout layout({
///       {0, 3, GL_FLOAT, GL_FALSE},   // position: vec3
///       {1, 3, GL_FLOAT, GL_FALSE},   // color:    vec3
///   });
///   layout.Apply();
class VertexLayout {
public:
    /// Constructs the layout from a list of attributes.
    /// Stride and per-attribute byte offsets are computed automatically.
    explicit VertexLayout(std::vector<VertexAttribute> attributes);

    /// Calls glVertexAttribPointer and glEnableVertexAttribArray for every attribute.
    /// Must be called while the correct VAO and VBO are bound.
    void Apply() const;

    /// Returns the total size in bytes of one vertex (sum of all attribute sizes).
    GLsizei GetStride() const { return m_stride; }

private:
    std::vector<VertexAttribute> m_attributes;
    GLsizei m_stride = 0; ///< Total byte size of one vertex, computed in constructor
};