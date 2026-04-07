// Inputs from vertex stage
in vec3 v_Color;

// Frag output
out vec4 FragColor;

void main()
{
    FragColor = vec4(v_Color, 1.0);
}