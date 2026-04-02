layout(std140, binding = 0) uniform Camera {
    mat4 view;
    mat4 projection;
    mat4 viewProj;
    vec3 cameraPos;
    float _pad0;
};

// Vertex inputs
// These locations must match the VertexLayout passed to MeshBuffer.
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Color;

// Per-object model matrix set by renderer
uniform mat4 u_Model;

// Outputs to fragment stage
out vec3 v_Color;

void main()
{
    v_Color = a_Color;
    gl_Position = projection * view * u_Model * vec4(a_Position, 1.0);
}