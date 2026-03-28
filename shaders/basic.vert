#version 460 core

// Vertex inputs
// These locations must match the VertexLayout passed to MeshBuffer.
// Example layout: {0, 3, GL_FLOAT, GL_FALSE} for position
//                 {1, 3, GL_FLOAT, GL_FALSE} for color
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Color;

// Outputs to fragment stage
out vec3 v_Color;

// Uniforms
// MVP matrices will be added in Sprint 3 when the camera/transform system exists.
// For now the shader draws in clip space directly (no transform).

void main()
{
    v_Color = a_Color;
    gl_Position = vec4(a_Position, 1.0);
}
