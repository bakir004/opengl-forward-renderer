in vec3 v_Normal;
in vec3 v_WorldPos;
in vec2 v_UV;
// --- ADDED --- Fragment position in light space for shadow sampling
in vec4 v_LightSpacePos;

layout(std140, binding = 0) uniform Camera {
    mat4 view;
    mat4 projection;
    mat4 viewProj;
    vec3 cameraPos;
    float _pad0;
};

#include "light_block.glsl"

uniform sampler2D u_AlbedoMap;
// --- ADDED --- Shadow map texture
uniform sampler2D u_ShadowMap;
uniform vec4      u_TintColor = vec4(1.0);
uniform float     u_Shininess = 64.0;
uniform float     u_SpecularStrength = 0.35;

out vec4 FragColor;

float DiffuseTerm(vec3 n, vec3 l)
{
    // Lambert diffuse keeps only front-facing light contribution.
    return max(dot(n, l), 0.0);
}

float BlinnPhongSpecular(vec3 n, vec3 l, vec3 v, float shininess)
{
    // Blinn-Phong uses the half-vector for stable highlights.
    vec3 h = normalize(l + v);
    return pow(max(dot(n, h), 0.0), shininess);
}

// --- ADDED --- Calculate shadow factor with PCF (3x3 kernel)
float CalculateShadow(vec4 lightSpacePos)
{
    // Perspective divide to get NDC
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    
    // Transform from [-1,1] to [0,1]
    projCoords = projCoords * 0.5 + 0.5;
    
    // Clamp to shadow map bounds to avoid sampling outside
    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || 
        projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0; // No shadow outside shadow map
    
    // Get closest depth from shadow map
    float closestDepth = texture(u_ShadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;
    
    // Bias to prevent shadow acne
    float bias = 0.005;
    
    // PCF: 3x3 kernel for soft shadows
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(u_ShadowMap, 0);
    
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(u_ShadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += (currentDepth - bias) > pcfDepth ? 0.5 : 0.0; // 0.5 for soft 3x3
        }
    }
    shadow /= 9.0; // Average 9 samples
    
    return shadow;
}

float DistanceAttenuation(float radius,
                          float attnConstant,
                          float attnLinear,
                          float attnQuadratic,
                          float distanceToLight)
{
    // Hard cutoff at max light radius, then smooth fade near the edge.
    if (distanceToLight >= radius)
        return 0.0;

    float denom = attnConstant
        + attnLinear * distanceToLight
        + attnQuadratic * distanceToLight * distanceToLight;
    float physical = 1.0 / max(denom, 0.0001);

    float normalized = 1.0 - (distanceToLight / radius);
    float smoothFalloff = normalized * normalized;
    return physical * smoothFalloff;
}

vec3 DirectionalLighting(vec3 albedo, vec3 n, vec3 v)
{
    // Directional light has no source position: only direction + radiance.
    if (u_HasDirectional == 0 || u_Directional.enabled == 0u)
        return vec3(0.0);

    vec3 l = normalize(-u_Directional.direction);
    float diffuse = DiffuseTerm(n, l);
    float spec = BlinnPhongSpecular(n, l, v, u_Shininess);
    vec3 irradiance = u_Directional.color * u_Directional.intensity;

    vec3 diffuseColor = albedo * irradiance * diffuse;
    vec3 specColor = irradiance * (u_SpecularStrength * spec);
    
    // --- ADDED --- Apply directional shadow
    float shadow = CalculateShadow(v_LightSpacePos);
    vec3 result = (diffuseColor + specColor) * (1.0 - shadow);
    
    return result;
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
        float attenuation = DistanceAttenuation(
            light.radius,
            light.attnConstant,
            light.attnLinear,
            light.attnQuadratic,
            d);
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

vec3 SpotLighting(vec3 albedo, vec3 worldPos, vec3 n, vec3 v)
{
    vec3 sum = vec3(0.0);

    int count = clamp(u_NumSpotLights, 0, 8);
    for (int i = 0; i < count; ++i) {
        GpuSpotLight light = u_SpotLights[i];
        if (light.enabled == 0u)
            continue;

        vec3 toLight = light.position - worldPos;
        float d = length(toLight);
        if (d <= 0.0001)
            continue;

        vec3 l = toLight / d;

        float attenuation = DistanceAttenuation(
            light.radius,
            light.attnConstant,
            light.attnLinear,
            light.attnQuadratic,
            d);
        if (attenuation <= 0.0)
            continue;

        // Spot direction points away from source; compare against source->fragment.
        vec3 spotDir = normalize(light.direction);
        vec3 sourceToFrag = normalize(worldPos - light.position);
        float cosTheta = dot(spotDir, sourceToFrag);

        if (cosTheta <= light.outerCos)
            continue;

        // Smooth blend: full intensity inside inner cone, zero at/after outer cone.
        float coneBlend = smoothstep(light.outerCos, light.innerCos, cosTheta);
        float weight = attenuation * coneBlend;

        float diffuse = DiffuseTerm(n, l);
        float spec = BlinnPhongSpecular(n, l, v, u_Shininess);
        vec3 irradiance = light.color * light.intensity * weight;

        vec3 diffuseColor = albedo * irradiance * diffuse;
        vec3 specColor = irradiance * (u_SpecularStrength * spec);
        sum += diffuseColor + specColor;
    }

    return sum;
}

void main()
{
    // Base color remains material-driven; lighting modulates this albedo.
    vec4 texel = texture(u_AlbedoMap, v_UV) * u_TintColor;
    vec3 albedo = texel.rgb;

    // Dev 5 core vectors for Blinn-Phong shading.
    vec3 n = normalize(v_Normal);
    vec3 v = normalize(cameraPos - v_WorldPos);

    // Ambient + directional + local lights.
    vec3 ambient = albedo * u_AmbientColor * u_AmbientIntensity;
    vec3 directional = DirectionalLighting(albedo, n, v);
    vec3 point = PointLighting(albedo, v_WorldPos, n, v);
    vec3 spot = SpotLighting(albedo, v_WorldPos, n, v);

    vec3 litColor = ambient + directional + point + spot;
    FragColor = vec4(litColor, texel.a);
}