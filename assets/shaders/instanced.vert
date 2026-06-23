// Instanced vertex shader — reads the per-instance model matrix from four
// instanced vertex attributes. Varyings match mesh.vert so mesh.frag can be
// reused unchanged as the fragment stage. Compatible with OpenGL 4.1.

layout(std140) uniform Camera {
    mat4 view;
    mat4 projection;
    mat4 viewProj;
    vec3 cameraPos;
    float _pad0;
};

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_UV;
layout(location = 3) in vec4 a_Tangent;
layout(location = 6) in mat4 a_InstanceTransform;

out vec3  v_Normal;
out vec3  v_WorldPos;
out vec2  v_UV;
out mat3  v_TBN;
out float v_ViewDepth;

void main()
{
    mat4 model = a_InstanceTransform;

    mat3 normalMatrix  = transpose(inverse(mat3(model)));
    vec3 worldNormal   = normalize(normalMatrix * a_Normal);
    vec3 worldTangent  = normalize(mat3(model) * a_Tangent.xyz);
    worldTangent       = normalize(worldTangent - worldNormal * dot(worldTangent, worldNormal));
    vec3 worldBitangent = normalize(cross(worldNormal, worldTangent)) * a_Tangent.w;

    v_Normal   = worldNormal;
    v_TBN      = mat3(worldTangent, worldBitangent, worldNormal);
    v_WorldPos = (model * vec4(a_Position, 1.0)).xyz;
    v_UV       = a_UV;

    vec4 viewPos = view * vec4(v_WorldPos, 1.0);
    v_ViewDepth  = -viewPos.z;
    gl_Position  = projection * viewPos;
}
