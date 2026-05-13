in vec3 v_TexCoords;

uniform samplerCube u_EnvironmentMap;
uniform float u_Roughness;

#include "pbr_helpers.glsl"
#include "ibl_common.glsl"

out vec4 FragColor;

void main()
{
    vec3 n = normalize(v_TexCoords);
    vec3 r = n;
    vec3 v = r;

    const uint sampleCount = 1024u;
    vec3 prefilteredColor = vec3(0.0);
    float totalWeight = 0.0;

    for (uint i = 0u; i < sampleCount; ++i)
    {
        vec2 xi = Hammersley(i, sampleCount);
        vec3 h = ImportanceSampleGGX(xi, n, u_Roughness);
        vec3 l = normalize(2.0 * dot(v, h) * h - v);

        float nDotL = max(dot(n, l), 0.0);
        if (nDotL > 0.0)
        {
            prefilteredColor += textureLod(u_EnvironmentMap, l, 0.0).rgb * nDotL;
            totalWeight += nDotL;
        }
    }

    prefilteredColor = prefilteredColor / max(totalWeight, 0.0001);
    FragColor = vec4(prefilteredColor, 1.0);
}