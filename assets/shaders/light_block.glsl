// light_block.glsl
// Include in every lit vertex/fragment shader with:
//   #include "light_block.glsl"
//
// Binding 0 is reserved for CameraBlock (Camera.h / basic.vert).
// Binding 1 is this block — the per-frame light payload.
//
// Sizes (std140):
//   GpuDirectionalLight  48 bytes
//   GpuPointLight        48 bytes
//   GpuSpotLight         80 bytes
//   LightBlock total   1488 bytes

struct GpuDirectionalLight {
    vec3  direction;      // normalized world-space direction toward the light
    float intensity;
    vec3  color;
    uint  enabled;
    uint  castShadow;
    float depthBias;
    float normalBias;
    float slopeBias;      // slope-scaled multiplier for depth bias
};

struct GpuPointLight {
    vec3  position;
    float intensity;
    vec3  color;
    float radius;         // attenuation reaches zero at this world-unit distance
    float attnConstant;
    float attnLinear;
    float attnQuadratic;
    uint  enabled;
};

struct GpuSpotLight {
    vec3  position;
    float intensity;
    vec3  direction;      // normalized, points away from light source
    float radius;
    vec3  color;
    float innerCos;       // cos(innerDeg) — pre-computed on CPU
    float outerCos;       // cos(outerDeg)
    float attnConstant;
    float attnLinear;
    float attnQuadratic;
    uint  enabled;
    float _pad0;
    float _pad1;
    float _pad2;
};

layout(std140, binding = 1) uniform LightBlock {
    vec3  u_AmbientColor;
    float u_AmbientIntensity;

    int   u_NumPointLights;
    int   u_NumSpotLights;
    int   u_HasDirectional;
    float _lightPad0;

    GpuDirectionalLight u_Directional;
    GpuPointLight       u_PointLights[16];
    GpuSpotLight        u_SpotLights[8];
};
