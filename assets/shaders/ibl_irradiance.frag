in vec3 v_TexCoords;

#include "pbr_helpers.glsl"

uniform samplerCube u_EnvironmentMap;

out vec4 FragColor;

void main()
{
    vec3 n = normalize(v_TexCoords);
    vec3 up = abs(n.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 right = normalize(cross(up, n));
    up = cross(n, right);

    vec3 irradiance = vec3(0.0);
    float sampleCount = 0.0;
    const float sampleDelta = 0.025;

    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
    {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
        {
            vec3 tangentSample = vec3(
                sin(theta) * cos(phi),
                sin(theta) * sin(phi),
                cos(theta));

            vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * n;
            irradiance += texture(u_EnvironmentMap, normalize(sampleVec)).rgb * cos(theta) * sin(theta);
            sampleCount += 1.0;
        }
    }

    irradiance = PI * irradiance * (1.0 / max(sampleCount, 1.0));
    FragColor = vec4(irradiance, 1.0);
}