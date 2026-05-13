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

// Texture units (must match C++ MaterialTextureUnit / EnvironmentTextureUnit):
//   0–5  material maps (albedo, normal, metallic, roughness, AO, emissive)
//   6    specular/glossiness (when used)
//   7    cascaded shadow map array
//   8    u_IrradianceMap   (diffuse IBL)
//   9    u_PrefilteredMap  (specular prefiltered environment)
//   10   u_BRDFLUT         (split-sum BRDF; RG = scale, bias — wired in Task 7)
uniform sampler2D      u_AlbedoMap;
uniform sampler2D      u_NormalMap;
uniform sampler2D      u_MetallicMap;
uniform sampler2D      u_RoughnessMap;
uniform sampler2D      u_AOMap;
uniform sampler2D      u_EmissiveMap;
uniform sampler2D      u_SpecularGlossinessMap;
uniform sampler2DArray u_CascadeShadowMaps;

uniform samplerCube    u_IrradianceMap;
uniform samplerCube    u_PrefilteredMap;
uniform sampler2D      u_BRDFLUT;
uniform bool           u_HasIrradianceMap = false;
uniform bool           u_HasPrefilteredMap = false;
uniform bool           u_HasBRDFLUT = false;
uniform bool           u_HasIBL = false;
uniform float          u_IBLIntensity     = 1.0;

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
uniform bool           u_HasSpecularGlossinessMap = false;
uniform bool           u_IsSpecularGlossiness = false;
uniform bool           u_UseNormalMap = true;
uniform vec4           u_TintColor = vec4(1.0);
uniform vec3           u_AlbedoColor = vec3(1.0);
uniform float          u_MetallicValue = 0.0;
uniform float          u_RoughnessValue = 0.5;
uniform float          u_AoStrength = 1.0;
uniform vec3           u_EmissiveColor = vec3(0.0);
uniform float          u_EmissiveStrength = 1.0;
uniform vec3           u_SpecularFactor = vec3(1.0);
uniform float          u_GlossinessFactor = 1.0;
uniform float          u_NormalScale = 1.0;
uniform float          u_FlipNormalMapY = 0.0; // 1.0 = DirectX-convention normal map (green channel inverted)

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
    float metallic = u_MetallicValue;
    if (u_HasMetallicMap)
        metallic *= texture(u_MetallicMap, v_UV).r;
    return clamp(metallic, 0.0, 1.0);
}

float GetRoughness()
{
    float roughness = u_RoughnessValue;
    if (u_HasRoughnessMap)
        roughness *= texture(u_RoughnessMap, v_UV).r;
    return clamp(roughness, 0.04, 1.0);
}

float GetAO()
{
    float ao = 1.0;
    if (u_HasAoMap)
        ao = texture(u_AOMap, v_UV).r;
    return mix(1.0, ao, u_AoStrength);
}

vec3 GetEmissive()
{
    vec3 emissive = u_EmissiveColor;
    if (u_HasEmissiveMap)
        emissive *= texture(u_EmissiveMap, v_UV).rgb;
    return emissive * u_EmissiveStrength;
}

vec3 GetSpecular()
{
    if (u_HasSpecularGlossinessMap)
        return texture(u_SpecularGlossinessMap, v_UV).rgb * u_SpecularFactor;
    return u_SpecularFactor;
}

float GetGlossiness()
{
    if (u_HasSpecularGlossinessMap)
        return texture(u_SpecularGlossinessMap, v_UV).a * u_GlossinessFactor;
    return u_GlossinessFactor;
}

vec3 ResolveWorldNormal()
{
    vec3 worldNormal = normalize(v_Normal);
    if (!u_HasNormalMap || !u_UseNormalMap)
        return worldNormal;

    vec3 tangentNormal = texture(u_NormalMap, v_UV).xyz * 2.0 - 1.0;

    // DirectX normal maps store Y inverted relative to OpenGL convention.
    // Flip green channel when loading NormalDX assets (e.g. Poly Haven textures).
    if (u_FlipNormalMapY > 0.5)
        tangentNormal.y = -tangentNormal.y;

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
                       vec3 F0,
                       float roughness)
{
    vec3 H = normalize(V + L);
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    if (NdotV <= 0.0 || NdotL <= 0.0)
        return vec3(0.0);

    float D = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 0.0001);
    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS);
    // Note: metallic workflow would multiply kD by (1 - metallic) outside or here.
    // For general PBRLight, we assume albedo is already pre-multiplied if needed or handled by caller.
    vec3 diffuse = kD * albedo / PI;

    return (diffuse + specular) * radiance * NdotL;
}

vec3 DirectionalLighting(vec3 albedo, vec3 n, vec3 v, vec3 F0, float roughness, float metallic)
{
    // Directional light now uses Cook-Torrance BRDF with existing light radiance.
    if (u_HasDirectional == 0 || u_Directional.enabled == 0u)
        return vec3(0.0);

    vec3 l = normalize(-u_Directional.direction);
    vec3 radiance = u_Directional.color * u_Directional.intensity;
    
    // Adjust albedo for metallic workflow if not spec-gloss
    vec3 actualAlbedo = albedo;
    if (!u_IsSpecularGlossiness) {
        actualAlbedo *= (1.0 - metallic);
    }

    vec3 directLight = CalculatePBRLight(n, v, l, radiance, actualAlbedo, F0, roughness);

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
                   vec3 F0,
                   float roughness,
                   float metallic)
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
        
        vec3 actualAlbedo = albedo;
        if (!u_IsSpecularGlossiness) {
            actualAlbedo *= (1.0 - metallic);
        }

        Lo += CalculatePBRLight(n, v, l, radiance, actualAlbedo, F0, roughness);
    }

    return Lo;
}

vec3 SpotLighting(vec3 albedo,
                  vec3 worldPos,
                  vec3 n,
                  vec3 v,
                  vec3 F0,
                  float roughness,
                  float metallic)
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
        
        vec3 actualAlbedo = albedo;
        if (!u_IsSpecularGlossiness) {
            actualAlbedo *= (1.0 - metallic);
        }

        Lo += CalculatePBRLight(n, v, l, radiance, actualAlbedo, F0, roughness);
    }

    return Lo;
}

void main()
{
    vec4 albedoSample = GetAlbedoSample();
    vec3 albedo = GetAlbedo() * u_TintColor.rgb;
    float alpha = albedoSample.a * u_TintColor.a;
    
    float metallic;
    float roughness;
    vec3 F0;

    if (u_IsSpecularGlossiness) {
        vec3 specular = GetSpecular();
        float glossiness = GetGlossiness();
        F0 = specular;
        roughness = 1.0 - glossiness;
        metallic = 0.0; // Not used in spec-gloss logic but for uniform's sake
    } else {
        metallic = GetMetallic();
        roughness = GetRoughness();
        F0 = mix(vec3(0.04), albedo, metallic);
    }

    float ao = GetAO();
    vec3 emissive = GetEmissive();

    // Use normalized world-space normal and the fragment-to-camera view vector for BRDF evaluation.
    vec3 N = ResolveWorldNormal();
    vec3 V = normalize(cameraPos - v_WorldPos);

    // Existing scene ambient stays in place; Lo collects direct-light BRDF contributions.
    vec3 kS = FresnelSchlick(max(dot(N, V), 0.0), F0);
    
    vec3 kD;
    if (u_IsSpecularGlossiness) {
        kD = (vec3(1.0) - kS);
    } else {
        kD = (vec3(1.0) - kS) * (1.0 - metallic);
    }

    vec3 ambient;
    if (u_HasIrradianceMap)
    {
        vec3 irradiance    = texture(u_IrradianceMap, N).rgb;
        vec3 diffuseIBL    = irradiance * albedo;          // kD already in kD below
        vec3 iblDiffuse    = kD * diffuseIBL * ao * u_IBLIntensity;

        // Blend IBL ambient with the scene ambient so scenes without a probe
        // still look correct (scene ambient acts as a floor).
        vec3 sceneAmbient  = kD * albedo * u_AmbientColor * u_AmbientIntensity * ao;
        ambient = max(iblDiffuse, sceneAmbient);
    }
    else
    {
        ambient = kD * albedo * u_AmbientColor * u_AmbientIntensity * ao;
    }

    vec3 Lo = DirectionalLighting(albedo, N, V, F0, roughness, metallic);
    Lo += PointLighting(albedo, v_WorldPos, N, V, F0, roughness, metallic);
    Lo += SpotLighting(albedo, v_WorldPos, N, V, F0, roughness, metallic);

    FragColor = vec4(ambient + Lo + emissive, alpha);
}
