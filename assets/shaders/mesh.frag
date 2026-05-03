const int NUM_CASCADES = 4;

in vec3  v_Normal;
in vec3  v_WorldPos;
in vec2  v_UV;
in mat3  v_TBN;
in float v_ViewDepth;

layout(std140, binding = 0) uniform Camera {
    mat4 view;
    mat4 projection;
    mat4 viewProj;
    vec3 cameraPos;
    float _pad0;
};

#include "light_block.glsl"
#include "pbr_helpers.glsl"

uniform sampler2D      u_AlbedoMap;
uniform sampler2D      u_NormalMap;
uniform sampler2D      u_MetallicMap;
uniform sampler2D      u_RoughnessMap;
uniform sampler2D      u_AOMap;
uniform sampler2D      u_EmissiveMap;
uniform sampler2DArray u_CascadeShadowMaps;
uniform mat4           u_CascadeViewProj[NUM_CASCADES];
uniform float          u_CascadeSplits[NUM_CASCADES];
uniform int            u_PCFRadius = 1;
uniform int            u_ReceiveShadow = 1;
uniform bool           u_HasAlbedoMap = false;
uniform bool           u_HasNormalMap = false;
uniform bool           u_HasMetallicMap = false;
uniform bool           u_HasRoughnessMap = false;
uniform bool           u_HasAoMap = false;
uniform bool           u_HasEmissiveMap = false;
uniform vec4           u_TintColor = vec4(1.0);
uniform vec3           u_AlbedoColor = vec3(1.0);
uniform float          u_MetallicValue = 0.0;
uniform float          u_RoughnessValue = 0.5;
uniform vec3           u_EmissiveColor = vec3(0.0);
uniform float          u_NormalScale = 1.0;

out vec4 FragColor;

vec4 GetAlbedoSample()
{
    if (u_HasAlbedoMap)
        return texture(u_AlbedoMap, v_UV);
    return vec4(u_AlbedoColor, 1.0);
}

vec3 GetAlbedo()
{
    return GetAlbedoSample().rgb;
}

float GetMetallic()
{
    if (u_HasMetallicMap)
        return texture(u_MetallicMap, v_UV).r;
    return clamp(u_MetallicValue, 0.0, 1.0);
}

float GetRoughness()
{
    if (u_HasRoughnessMap)
        return texture(u_RoughnessMap, v_UV).r;
    return clamp(u_RoughnessValue, 0.04, 1.0);
}

float GetAO()
{
    if (u_HasAoMap)
        return texture(u_AOMap, v_UV).r;
    return 1.0;
}

vec3 GetEmissive()
{
    if (u_HasEmissiveMap)
        return texture(u_EmissiveMap, v_UV).rgb * u_EmissiveColor;
    return u_EmissiveColor;
}

vec3 ResolveWorldNormal()
{
    vec3 worldNormal = normalize(v_Normal);
    if (!u_HasNormalMap)
        return worldNormal;

    vec3 tangentNormal = texture(u_NormalMap, v_UV).xyz * 2.0 - 1.0;
    tangentNormal.xy *= u_NormalScale;
    tangentNormal.z = sqrt(max(1.0 - dot(tangentNormal.xy, tangentNormal.xy), 0.0));
    tangentNormal = normalize(tangentNormal);

    return normalize(v_TBN * tangentNormal);
}

// Picks the tightest cascade that still covers this fragment's view-space depth.
int SelectCascade(float viewDepth)
{
    for (int i = 0; i < NUM_CASCADES; ++i) {
        if (viewDepth < u_CascadeSplits[i])
            return i;
    }
    return NUM_CASCADES - 1;
}

// Fraction of each cascade's range used as a blend zone into the next cascade.
// Inside this zone we sample both cascades and lerp; outside, only one.
const float CASCADE_BLEND_FRACTION = 0.15;

// Samples one cascade with PCF. Returns [0,1] occlusion where 1 is fully shadowed.
// The normal is used for normal-offset bias: the world position is shifted
// along the surface normal before the light-space projection, which moves the
// shadow test point away from the surface and reduces both acne and peter-panning
// more gracefully than depth bias alone. The offset grows on far cascades to
// match their larger texel footprint.
float SampleCascade(int cascade, vec3 worldPos, vec3 normal, float bias)
{
    float normalOffsetScale = u_Directional.normalBias * (1.0 + float(cascade) * 0.5);
    vec3  biasedWorldPos    = worldPos + normal * normalOffsetScale;

    vec4 lightSpacePos = u_CascadeViewProj[cascade] * vec4(biasedWorldPos, 1.0);
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;

    vec2 texelSize = 1.0 / vec2(textureSize(u_CascadeShadowMaps, 0).xy);
    float currentDepth = projCoords.z;

    int radius = clamp(u_PCFRadius, 0, 4);
    if (radius == 0) {
        float d = texture(u_CascadeShadowMaps,
                          vec3(projCoords.xy, float(cascade))).r;
        return (currentDepth - bias) > d ? 1.0 : 0.0;
    }

    float shadow = 0.0;
    float samples = 0.0;
    for (int x = -radius; x <= radius; ++x) {
        for (int y = -radius; y <= radius; ++y) {
            vec2 offset = vec2(x, y) * texelSize;
            float pcfDepth = texture(
                u_CascadeShadowMaps,
                vec3(projCoords.xy + offset, float(cascade))).r;
            shadow += (currentDepth - bias) > pcfDepth ? 1.0 : 0.0;
            samples += 1.0;
        }
    }
    return shadow / samples;
}

float CascadeBias(int cascade, float slope)
{
    // Base bias comes from u_Directional.depthBias (runtime-tunable in debug UI).
    // The slope term grows the bias on surfaces facing away from the light to
    // prevent shadow acne, and far cascades loosen further because their texels
    // are larger and more prone to self-shadow artifacts.
    float baseBias  = u_Directional.depthBias;
    float slopeBias = baseBias * u_Directional.slopeBias * slope;
    float bias      = max(slopeBias, baseBias * 0.3);
    return bias * (1.0 + float(cascade) * 0.75);
}

// Picks the tightest cascade for this fragment, samples it with PCF, and
// cross-fades into the next cascade near the split boundary so the transition
// isn't visibly stepped.
float CalculateShadow(vec3 worldPos, vec3 normal, vec3 lightDir, float viewDepth)
{
    int cascade = SelectCascade(viewDepth);

    float slope = 1.0 - max(dot(normal, lightDir), 0.0);
    float bias  = CascadeBias(cascade, slope);
    float shadow = SampleCascade(cascade, worldPos, normal, bias);

    // Only blend forward if there is a next cascade to blend into.
    if (cascade < NUM_CASCADES - 1) {
        float splitNear = (cascade == 0) ? 0.0 : u_CascadeSplits[cascade - 1];
        float splitFar  = u_CascadeSplits[cascade];
        float blendStart = mix(splitFar, splitNear, CASCADE_BLEND_FRACTION);

        if (viewDepth > blendStart) {
            float t = (viewDepth - blendStart) / max(splitFar - blendStart, 0.0001);
            t = clamp(t, 0.0, 1.0);
            // Smoothstep feels more natural than a linear ramp for shadow blends.
            t = t * t * (3.0 - 2.0 * t);

            float nextBias   = CascadeBias(cascade + 1, slope);
            float nextShadow = SampleCascade(cascade + 1, worldPos, normal, nextBias);
            shadow = mix(shadow, nextShadow, t);
        }
    }
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

vec3 CalculatePBRLight(vec3 N,
                       vec3 V,
                       vec3 L,
                       vec3 radiance,
                       vec3 albedo,
                       float metallic,
                       float roughness)
{
    vec3 H = normalize(V + L);
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    if (NdotV <= 0.0 || NdotL <= 0.0)
        return vec3(0.0);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    float D = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 0.0001);
    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / PI;

    return (diffuse + specular) * radiance * NdotL;
}

vec3 DirectionalLighting(vec3 albedo, vec3 n, vec3 v, float metallic, float roughness)
{
    // Directional light now uses Cook-Torrance BRDF with existing light radiance.
    if (u_HasDirectional == 0 || u_Directional.enabled == 0u)
        return vec3(0.0);

    vec3 l = normalize(-u_Directional.direction);
    vec3 radiance = u_Directional.color * u_Directional.intensity;
    vec3 directLight = CalculatePBRLight(n, v, l, radiance, albedo, metallic, roughness);

    float shadow = 0.0;
    if (u_ReceiveShadow != 0) {
        shadow = CalculateShadow(v_WorldPos, n, l, v_ViewDepth);
    }
    return directLight * (1.0 - shadow);
}

vec3 PointLighting(vec3 albedo,
                   vec3 worldPos,
                   vec3 n,
                   vec3 v,
                   float metallic,
                   float roughness)
{
    vec3 Lo = vec3(0.0);

    int count = clamp(u_NumPointLights, 0, 16);
    for (int i = 0; i < count; ++i) {
        GpuPointLight light = u_PointLights[i];
        if (light.enabled == 0u)
            continue;

        vec3 lightVector = light.position - worldPos;
        float distanceToLight = length(lightVector);
        if (distanceToLight <= 0.0001)
            continue;

        vec3 l = lightVector / distanceToLight;
        float attenuation = DistanceAttenuation(
            light.radius,
            light.attnConstant,
            light.attnLinear,
            light.attnQuadratic,
            distanceToLight);
        if (attenuation <= 0.0)
            continue;

        vec3 radiance = light.color * light.intensity * attenuation;
        Lo += CalculatePBRLight(n, v, l, radiance, albedo, metallic, roughness);
    }

    return Lo;
}

vec3 SpotLighting(vec3 albedo,
                  vec3 worldPos,
                  vec3 n,
                  vec3 v,
                  float metallic,
                  float roughness)
{
    vec3 Lo = vec3(0.0);

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
        vec3 radiance = light.color * light.intensity * weight;
        Lo += CalculatePBRLight(n, v, l, radiance, albedo, metallic, roughness);
    }

    return Lo;
}

void main()
{
    vec4 albedoSample = GetAlbedoSample();
    vec3 albedo = GetAlbedo() * u_TintColor.rgb;
    float alpha = albedoSample.a * u_TintColor.a;
    float metallic = GetMetallic();
    float roughness = GetRoughness();
    float ao = GetAO();
    vec3 emissive = GetEmissive();

    // Use normalized world-space normal and the fragment-to-camera view vector for BRDF evaluation.
    vec3 N = ResolveWorldNormal();
    vec3 V = normalize(cameraPos - v_WorldPos);

    // Existing scene ambient stays in place; Lo collects direct-light BRDF contributions.
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 kS = FresnelSchlick(max(dot(N, V), 0.0), F0);
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
    vec3 ambient = kD * albedo * u_AmbientColor * u_AmbientIntensity * ao;
    vec3 Lo = DirectionalLighting(albedo, N, V, metallic, roughness);
    Lo += PointLighting(albedo, v_WorldPos, N, V, metallic, roughness);
    Lo += SpotLighting(albedo, v_WorldPos, N, V, metallic, roughness);

    FragColor = vec4(ambient + Lo + emissive, alpha);
}
