in vec2 v_UV;

#include "pbr_helpers.glsl"
#include "ibl_common.glsl"

out vec2 FragColor;

vec2 IntegrateBRDF(float nDotV, float roughness)
{
    const uint sampleCount = 1024u;
    vec3 n = vec3(0.0, 0.0, 1.0);
    vec3 v = vec3(sqrt(max(1.0 - nDotV * nDotV, 0.0)), 0.0, nDotV);

    float a = 0.0;
    float b = 0.0;

    for (uint i = 0u; i < sampleCount; ++i)
    {
        vec2 xi = Hammersley(i, sampleCount);
        vec3 h = ImportanceSampleGGX(xi, n, roughness);
        vec3 l = normalize(2.0 * dot(v, h) * h - v);

        float nDotL = max(l.z, 0.0);
        float nDotH = max(h.z, 0.0);
        float vDotH = max(dot(v, h), 0.0);

        if (nDotL > 0.0)
        {
            float g = GeometrySmith(n, v, l, roughness);
            float gVis = (g * vDotH) / max(nDotH * nDotV, 0.0001);
            float fc = pow(1.0 - vDotH, 5.0);

            a += (1.0 - fc) * gVis;
            b += fc * gVis;
        }
    }

    return vec2(a, b) / float(sampleCount);
}

void main()
{
    FragColor = IntegrateBRDF(v_UV.x, v_UV.y);
}