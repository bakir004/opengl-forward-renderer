out vec4 FragColor;

in vec3 v_TexCoords;

uniform samplerCube u_Skybox;

void main()
{
    // Sampling the cubemap. Since we're in a forward renderer, 
    // we assume the cubemap is already in the correct color space.
    FragColor = texture(u_Skybox, v_TexCoords);
}
