in vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_HdrColor;
uniform float u_Threshold;
uniform float u_SoftKnee;
uniform float u_Exposure;

const vec3 LUMA = vec3(0.2126, 0.7152, 0.0722);

vec3 BrightExtract(vec3 color)
{
    vec3 col = color * u_Exposure;
    float lum = dot(col, LUMA);

    float knee = u_Threshold * u_SoftKnee;
    float soft = max(lum - u_Threshold + knee, 0.0);
    float softPart = (knee > 0.0) ? (soft * soft) / (4.0 * knee + 1e-6) : 0.0;

    float contrib = max(lum - u_Threshold, 0.0) + softPart;
    float factor = (lum > 1e-6) ? (contrib / lum) : 0.0;
    return col * factor;
}

void main()
{
    vec3 hdr = texture(u_HdrColor, v_TexCoord).rgb;
    vec3 bright = BrightExtract(hdr);
    FragColor = vec4(bright, 1.0);
}
