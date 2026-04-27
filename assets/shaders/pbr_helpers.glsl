const float PI = 3.14159265359;
const float MIN_ROUGHNESS = 0.001;
const float MIN_DENOMINATOR = 0.0001;

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float protectedRoughness = max(roughness, MIN_ROUGHNESS);
    float a = protectedRoughness * protectedRoughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    return a2 / max(PI * denom * denom, MIN_DENOMINATOR);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float protectedRoughness = max(roughness, MIN_ROUGHNESS);
    float clampedNdotV = clamp(NdotV, 0.0, 1.0);
    float r = protectedRoughness + 1.0;
    float k = (r * r) / 8.0;

    float denom = clampedNdotV * (1.0 - k) + k;
    return clampedNdotV / max(denom, MIN_DENOMINATOR);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggxV = GeometrySchlickGGX(NdotV, roughness);
    float ggxL = GeometrySchlickGGX(NdotL, roughness);
    return ggxV * ggxL;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    float clampedCosTheta = clamp(cosTheta, 0.0, 1.0);
    return F0 + (1.0 - F0) * pow(1.0 - clampedCosTheta, 5.0);
}
