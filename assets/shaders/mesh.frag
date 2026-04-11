in vec3 v_Normal;
in vec3 v_WorldPos;
in vec2 v_UV;

layout(std140, binding = 0) uniform Camera {
    mat4 view;
    mat4 projection;
    mat4 viewProj;
    vec3 cameraPos;
    float _pad0;
};

#include "light_block.glsl"

uniform sampler2D u_AlbedoMap;
uniform vec4      u_TintColor = vec4(1.0);
uniform float     u_Shininess = 64.0;
uniform float     u_SpecularStrength = 0.35;

out vec4 FragColor;

float DiffuseTerm(vec3 n, vec3 l)
{
    return max(dot(n, l), 0.0);
}

float BlinnPhongSpecular(vec3 n, vec3 l, vec3 v, float shininess)
{
    vec3 h = normalize(l + v);
    return pow(max(dot(n, h), 0.0), shininess);
}

float PointAttenuation(GpuPointLight light, float distanceToLight)
{
    if (distanceToLight >= light.radius)
        return 0.0;

    float denom = light.attnConstant
        + light.attnLinear * distanceToLight
        + light.attnQuadratic * distanceToLight * distanceToLight;
    float physical = 1.0 / max(denom, 0.0001);

    float normalized = 1.0 - (distanceToLight / light.radius);
    float smoothFalloff = normalized * normalized;
    return physical * smoothFalloff;
}

vec3 DirectionalLighting(vec3 albedo, vec3 n, vec3 v)
{
    if (u_HasDirectional == 0 || u_Directional.enabled == 0u)
        return vec3(0.0);

    vec3 l = normalize(-u_Directional.direction);
    float diffuse = DiffuseTerm(n, l);
    float spec = BlinnPhongSpecular(n, l, v, u_Shininess);
    vec3 irradiance = u_Directional.color * u_Directional.intensity;

    vec3 diffuseColor = albedo * irradiance * diffuse;
    vec3 specColor = irradiance * (u_SpecularStrength * spec);
    return diffuseColor + specColor;
}

vec3 PointLighting(vec3 albedo, vec3 worldPos, vec3 n, vec3 v)
{
    vec3 sum = vec3(0.0);

    int count = clamp(u_NumPointLights, 0, 16);
    for (int i = 0; i < count; ++i) {
        GpuPointLight light = u_PointLights[i];
        if (light.enabled == 0u)
            continue;

        vec3 toLight = light.position - worldPos;
        float d = length(toLight);
        if (d <= 0.0001)
            continue;

        vec3 l = toLight / d;
        float attenuation = PointAttenuation(light, d);
        if (attenuation <= 0.0)
            continue;

        float diffuse = DiffuseTerm(n, l);
        float spec = BlinnPhongSpecular(n, l, v, u_Shininess);
        vec3 irradiance = light.color * light.intensity * attenuation;

        vec3 diffuseColor = albedo * irradiance * diffuse;
        vec3 specColor = irradiance * (u_SpecularStrength * spec);
        sum += diffuseColor + specColor;
    }

    return sum;
}

void main()
{
    vec4 texel = texture(u_AlbedoMap, v_UV) * u_TintColor;
    vec3 albedo = texel.rgb;

    vec3 n = normalize(v_Normal);
    vec3 v = normalize(cameraPos - v_WorldPos);

    vec3 ambient = albedo * u_AmbientColor * u_AmbientIntensity;
    vec3 directional = DirectionalLighting(albedo, n, v);
    vec3 point = PointLighting(albedo, v_WorldPos, n, v);

    vec3 litColor = ambient + directional + point;
    FragColor = vec4(litColor, texel.a);
}
