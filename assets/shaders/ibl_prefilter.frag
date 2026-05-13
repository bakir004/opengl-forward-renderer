in vec3 v_TexCoords;

uniform samplerCube u_EnvironmentMap;
uniform float u_Roughness;

#include "pbr_helpers.glsl"
#include "ibl_common.glsl"

out vec4 FragColor;

void main()
{
    // For a cubemap convolution the normal, view, and reflection directions are
    // all the same: we assume the surface faces the sample direction directly.
    // This is the standard split-sum approximation where v = n = r.
    vec3 n = normalize(v_TexCoords);
    vec3 v = n;

    const uint sampleCount = 1024u;

    // Solid angle of one texel in the source environment cubemap.
    // textureSize() gives the face resolution at mip 0; the full sphere has
    // solid angle 4*PI spread across 6*res*res texels.
    float envResolution = float(textureSize(u_EnvironmentMap, 0).x);
    float saTexel = 4.0 * PI / (6.0 * envResolution * envResolution);

    vec3 prefilteredColor = vec3(0.0);
    float totalWeight     = 0.0;

    for (uint i = 0u; i < sampleCount; ++i)
    {
        // Generate a low-discrepancy sample in tangent space aligned to n.
        vec2 xi = Hammersley(i, sampleCount);
        vec3 h  = ImportanceSampleGGX(xi, n, u_Roughness);
        vec3 l  = normalize(2.0 * dot(v, h) * h - v);

        float nDotL = max(dot(n, l), 0.0);
        if (nDotL <= 0.0)
            continue;

        // ── Mip level selection ───────────────────────────────────────────────
        // Compute the GGX NDF value and the importance-sampling PDF for this
        // half-vector, then derive the solid angle of the sample cone.
        // When the sample's solid angle exceeds one texel's solid angle we step
        // into a blurrier mip level. This suppresses fireflies at high roughness
        // and avoids aliasing at low roughness.
        //
        // Reference: Karis, B. (2013) "Real Shading in Unreal Engine 4", SIGGRAPH.
        float nDotH = max(dot(n, h), 0.0);
        float hDotV = max(dot(h, v), 0.0);

        float D        = DistributionGGX(n, h, u_Roughness);
        float pdf      = max(D * nDotH / (4.0 * hDotV + 0.0001), 0.0001);
        float saSample = 1.0 / (float(sampleCount) * pdf + 0.0001);

        // At roughness 0 force mip 0 for a mirror-like result.
        float mipLevel = (u_Roughness == 0.0)
            ? 0.0
            : 0.5 * log2(saSample / saTexel);

        prefilteredColor += textureLod(u_EnvironmentMap, l, mipLevel).rgb * nDotL;
        totalWeight      += nDotL;
    }

    prefilteredColor = prefilteredColor / max(totalWeight, 0.0001);
    FragColor = vec4(prefilteredColor, 1.0);
}
