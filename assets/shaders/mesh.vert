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
layout(location = 3) in vec4 a_Tangent;

uniform mat4 u_Model;

out vec3 v_Normal;
out vec3 v_WorldPos;
out vec2 v_UV;
out mat3 v_TBN;
// View-space Z (positive, distance from camera) used for CSM cascade selection.
out float v_ViewDepth;

void main()
{
    mat3 normalMatrix = transpose(inverse(mat3(u_Model)));
    vec3 worldNormal = normalize(normalMatrix * a_Normal);

    vec3 worldTangent = normalize(mat3(u_Model) * a_Tangent.xyz);
    // Re-orthogonalize tangent against normal to keep TBN stable under non-uniform scale.
    worldTangent = normalize(worldTangent - worldNormal * dot(worldTangent, worldNormal));

    vec3 worldBitangent = normalize(cross(worldNormal, worldTangent)) * a_Tangent.w;

    v_Normal = worldNormal;
    v_TBN = mat3(worldTangent, worldBitangent, worldNormal);
    v_WorldPos = (u_Model * vec4(a_Position, 1.0)).xyz;
    v_UV       = a_UV;

    vec4 viewPos = view * vec4(v_WorldPos, 1.0);
    v_ViewDepth  = -viewPos.z;

    gl_Position = projection * viewPos;
}
