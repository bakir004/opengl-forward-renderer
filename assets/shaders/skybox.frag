out vec4 FragColor;

in vec3 v_TexCoords;

uniform samplerCube u_Skybox;
uniform vec3  u_Tint;
uniform float u_Exposure;
uniform vec3  u_EmissiveColor;
uniform float u_EmissiveStrength;

void main()
{
    vec3 color = texture(u_Skybox, v_TexCoords).rgb;
    color *= u_Tint * u_Exposure;
    color += u_EmissiveColor * u_EmissiveStrength;
    FragColor = vec4(color, 1.0);
}