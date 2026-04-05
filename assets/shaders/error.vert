layout(std140, binding = 0) uniform Camera {
    mat4 view;
    mat4 projection;
    mat4 viewProj;
    vec3 cameraPos;
    float _pad0;
};

// Error shader — only uses location 0 (position) so it works with both
// VertexPC (primitives) and VertexPNT (imported meshes) layouts.
layout(location = 0) in vec3 a_Position;

uniform mat4 u_Model;

out vec3 v_ObjectPos;

void main()
{
    v_ObjectPos = a_Position;
    gl_Position = projection * view * u_Model * vec4(a_Position, 1.0);
}
