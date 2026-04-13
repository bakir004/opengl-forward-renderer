layout(std140, binding = 0) uniform Camera {
    mat4 view;
    mat4 projection;
    mat4 viewProj;
    vec3 cameraPos;
    float _pad0;
};

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_UV;

uniform mat4 u_Model;

out vec3 v_Normal;
out vec3 v_WorldPos;
out vec2 v_UV;
// View-space Z (positive, distance from camera) used for CSM cascade selection.
out float v_ViewDepth;

void main()
{
    v_Normal = normalize(mat3(u_Model) * a_Normal);
    v_WorldPos = (u_Model * vec4(a_Position, 1.0)).xyz;
    v_UV       = a_UV;

    vec4 viewPos = view * vec4(v_WorldPos, 1.0);
    v_ViewDepth  = -viewPos.z;

    gl_Position = projection * viewPos;
}
