in vec2 v_TexCoord;

out vec4 FragColor;

uniform sampler2D u_HdrBuffer;
uniform sampler2D u_BloomBlur;
uniform float u_BloomStrength;
uniform bool u_BloomEnabled;
uniform float u_Exposure;
uniform bool u_ToneMappingEnabled;
uniform int u_ToneMappingOperator; // 0=Reinhard, 1=ACES, 2=Uncharted2
uniform int u_DebugView; // 0=Final, 1=HDR only, 2=Bright-pass, 3=Blurred bloom, 4=No bloom

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
    const float A = 0.15;
    const float B = 0.50;
    const float C = 0.10;
    const float D = 0.20;
    const float E = 0.02;
    const float F = 0.30;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 TonemapUncharted2(vec3 hdrColor, float exposure)
{
    vec3 color = hdrColor * exposure;
    vec3 curr = Uncharted2Curve(color);
    vec3 whitePoint = Uncharted2Curve(vec3(11.2));
    return curr / whitePoint;
}

vec3 ApplyToneMapping(vec3 hdrColor)
{
    if (!u_ToneMappingEnabled)
        return hdrColor * u_Exposure;

    if (u_ToneMappingOperator == 0)
        return TonemapReinhard(hdrColor, u_Exposure);
    if (u_ToneMappingOperator == 1)
        return TonemapACES(hdrColor, u_Exposure);
    return TonemapUncharted2(hdrColor, u_Exposure);
}

void main()
{
    vec3 hdrColor = texture(u_HdrBuffer, v_TexCoord).rgb;
    vec3 bloomColor = texture(u_BloomBlur, v_TexCoord).rgb;
    vec3 combinedHdr = hdrColor;
    if (u_BloomEnabled)
        combinedHdr += bloomColor * u_BloomStrength;

    vec3 outputColor = vec3(0.0);
    if (u_DebugView == 0)
    {
        outputColor = ApplyToneMapping(combinedHdr);
    }
    else if (u_DebugView == 1)
    {
        // Inspect scene HDR contribution only.
        outputColor = ApplyToneMapping(hdrColor);
    }
    else if (u_DebugView == 2)
    {
        outputColor = ApplyToneMapping(bloomColor);
    }
    else if (u_DebugView == 3)
    {
        outputColor = ApplyToneMapping(bloomColor);
    }
    else if (u_DebugView == 4)
    {
        outputColor = ApplyToneMapping(hdrColor);
    }
    else
    {
        outputColor = ApplyToneMapping(combinedHdr);
    }
    FragColor = vec4(outputColor, 1.0);
}
