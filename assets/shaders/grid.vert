#version 460 core

layout(location = 0) in vec3 a_Position;

layout(std140, binding = 0) uniform CameraBlock {
    mat4 u_View;
    mat4 u_Projection;
    vec3 u_CameraPosition;
};

out vec3 v_NearPoint;
out vec3 v_FarPoint;

vec3 UnprojectPoint(float x, float y, float z) {
    mat4 viewInv = inverse(u_View);
    mat4 projInv = inverse(u_Projection);
    vec4 unprojectedPoint = viewInv * projInv * vec4(x, y, z, 1.0);
    return unprojectedPoint.xyz / unprojectedPoint.w;
}

void main() {
    v_NearPoint = UnprojectPoint(a_Position.x, a_Position.y, 0.0);
    v_FarPoint = UnprojectPoint(a_Position.x, a_Position.y, 1.0);

    gl_Position = vec4(a_Position, 1.0);
}
