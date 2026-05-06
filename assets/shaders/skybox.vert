layout (location = 0) in vec3 a_Position;

out vec3 v_TexCoords;

uniform mat4 u_Projection;
uniform mat4 u_View;

void main()
{
    v_TexCoords = a_Position;
    // We want the skybox to be at the maximum depth (z=1.0 in NDC).
    // After perspective division (z/w), if we use xyww, z becomes w/w = 1.0.
    vec4 pos = u_Projection * u_View * vec4(a_Position, 1.0);
    gl_Position = pos.xyww;
}
