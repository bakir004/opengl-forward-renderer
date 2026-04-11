// Procedural magenta/black checkerboard drawn in object space.
// Stays fixed to the object (independent of camera) and scales with the mesh.
in vec3 v_ObjectPos;

out vec4 FragColor;

void main()
{
    vec3 tile = floor(v_ObjectPos / 0.25);
    float checker = mod(tile.x + tile.y + tile.z, 2.0);
    FragColor = mix(vec4(1.0, 0.0, 1.0, 1.0),   // magenta
                    vec4(0.0, 0.0, 0.0, 1.0),   // black
                    checker);
}
