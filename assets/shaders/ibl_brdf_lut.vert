layout(location = 0) in vec2 a_Position;

void main()
{
    // Fullscreen clip-space quad; LUT texel coordinates come from gl_FragCoord in the fragment shader.
    gl_Position = vec4(a_Position, 0.0, 1.0);
}