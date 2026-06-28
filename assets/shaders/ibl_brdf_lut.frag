// BRDF integration LUT (split-sum): each texel stores vec2(scale, bias) for specular IBL.
// Parameterization matches LearnOpenGL "Pre-computing the BRDF":
//   X = N·V, Y = roughness (see main(); coordinates from gl_FragCoord).

const float PI = 3.14159265359;

#include "ibl_common.glsl"

out vec2 FragColor;

// Schlick–GGX geometry term with the **IBL** remapping: k = α² / 2 (α = roughness).
// For direct lights many engines use k = (roughness + 1)^2 / 8 so the highlight stays
// sharper at grazing angles. IBL integrates over the hemisphere with a smoother response,
// so the literature uses the smaller k — otherwise the LUT would bake in too much masking.
float GeometrySchlickGGX_IBL(float NdotV, float roughness)
{
    float a = roughness;
    float k = (a * a) * 0.5;

    float denom = NdotV * (1.0 - k) + k;
    return NdotV / max(denom, 0.0001);
}

float GeometrySmith_IBL(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX_IBL(NdotV, roughness) * GeometrySchlickGGX_IBL(NdotL, roughness);
}

// Hammersley / ImportanceSampleGGX: low-discrepancy 2D points xi drive GGX half-vector sampling
// over the hemisphere tilted to N. That concentrates work where the NDF is large.

vec2 IntegrateBRDF(float NdotV, float roughness)
{
    const uint sampleCount = 1024u;

    vec3 N = vec3(0.0, 0.0, 1.0);
    // View direction in the tangent frame where N = +Z; only N·V matters for this integral.
    vec3 V = vec3(sqrt(max(1.0 - NdotV * NdotV, 0.0)), 0.0, NdotV);

    float A = 0.0;
    float B = 0.0;

    for (uint i = 0u; i < sampleCount; ++i)
    {
        vec2 xi = Hammersley(i, sampleCount);
        vec3 H = ImportanceSampleGGX(xi, N, roughness);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(L.z, 0.0);
        float NdotH = max(H.z, 0.0);
        float VdotH = max(dot(V, H), 0.0);

        if (NdotL > 0.0)
        {
            float G = GeometrySmith_IBL(N, V, L, roughness);
            float G_Vis = (G * VdotH) / max(NdotH * NdotV, 0.0001);
            float Fc = pow(1.0 - VdotH, 5.0);

            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }

    return vec2(A, B) / float(sampleCount);
}

void main()
{
    const float lutSize = 512.0;
    vec2 coord = gl_FragCoord.xy / lutSize;
    FragColor = IntegrateBRDF(coord.x, coord.y);
}
