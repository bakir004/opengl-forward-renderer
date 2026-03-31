#version 460 core

// Vertex inputs
// These locations must match the VertexLayout passed to MeshBuffer.
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Color;

// Outputs to fragment stage
out vec3 v_Color;

void main()
{
    v_Color = a_Color;
    gl_Position = vec4(a_Position, 1.0);
}