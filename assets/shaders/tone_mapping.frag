in vec2 v_TexCoord;

out vec4 FragColor;

uniform sampler2D u_HdrColor;
uniform int u_TonemapOperator; // 0=Reinhard, 1=ACES, 2=Uncharted2
uniform float u_Exposure;
uniform bool u_TonemapEnabled;

vec3 TonemapReinhard(vec3 hdrColor, float exposure)
{
    vec3 mapped = hdrColor * exposure;
    return mapped / (1.0 + mapped);
}

vec3 TonemapACES(vec3 hdrColor, float exposure)
{
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;

    vec3 color = hdrColor * exposure;
    color = (color * (a * color + b)) / (color * (c * color + d) + e);
    return clamp(color, 0.0, 1.0);
}

vec3 Uncharted2Curve(vec3 x)
{
    const float A = 0.15, B = 0.50, C = 0.10, 0.20, E = 0.02, F = 0.30;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 TonemapUncharted2(vec3 hdrColor, float exposure)
{
    vec3 color = hdrColor * exposure;
    vec3 curr = Uncharted2Curve(color);
    vec3 whitePoint = Uncharted2Curve(vec3(11.2));
    return curr / whitePoint;
}

vec3 ApplyGammaCorrection(vec3 color)
{
    return pow(max(color, vec3(0.0)), vec3(1.0 / 2.2));
}

void main()
{
    vec3 hdrColor = texture(u_HdrColor, v_TexCoord).rgb;
    vec3 result = hdrColor;

    if (u_TonemapEnabled)
    {
        if (u_TonemapOperator == 0)
            result = TonemapReinhard(hdrColor, u_Exposure);
        else if (u_TonemapOperator == 1)
            result = TonemapACES(hdrColor, u_Exposure);
        else
            result = TonemapUncharted2(hdrColor, u_Exposure);
    }
    else
    {
        result = hdrColor * u_Exposure;
    }

    FragColor = vec4(ApplyGammaCorrection(result), 1.0);
}
