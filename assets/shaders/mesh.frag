in vec3 v_Normal;
in vec2 v_UV;

uniform sampler2D u_AlbedoMap;
uniform vec4      u_TintColor = vec4(1.0);

out vec4 FragColor;

void main()
{
    FragColor = texture(u_AlbedoMap, v_UV) * u_TintColor;
}
