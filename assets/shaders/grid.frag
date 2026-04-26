#version 460 core

in vec3 v_NearPoint;
in vec3 v_FarPoint;

layout(std140, binding = 0) uniform CameraBlock {
    mat4 u_View;
    mat4 u_Projection;
    vec3 u_CameraPosition;
};

out vec4 FragColor;

uniform float u_GridScale;
uniform float u_GridMinorScale;
uniform float u_FadeDistance;
uniform float u_AxisThickness;

vec4 Grid(vec3 fragPos3D, float scale, bool drawMinor) {
    vec2 coord = fragPos3D.xz * scale;
    vec2 derivative = fwidth(coord);
    vec2 grid = abs(fract(coord - 0.5) - 0.5) / derivative;
    float line = min(grid.x, grid.y);
    float minimumz = min(derivative.y, 1.0);
    float minimumx = min(derivative.x, 1.0);

    vec4 color = vec4(0.2, 0.2, 0.2, 1.0 - min(line, 1.0));

    if (!drawMinor) {
        color.rgb = vec3(0.3, 0.3, 0.3);
        color.a *= 1.0;
    } else {
        color.a *= 0.5;
    }

    // X axis (red)
    if (abs(fragPos3D.z) < u_AxisThickness * scale) {
        color = vec4(1.0, 0.0, 0.0, 1.0);
    }
    // Z axis (blue)
    if (abs(fragPos3D.x) < u_AxisThickness * scale) {
        color = vec4(0.0, 0.0, 1.0, 1.0);
    }

    return color;
}

float ComputeDepth(vec3 pos) {
    vec4 clip_space_pos = u_Projection * u_View * vec4(pos.xyz, 1.0);
    return (clip_space_pos.z / clip_space_pos.w);
}

float ComputeLinearDepth(vec3 pos) {
    vec4 clip_space_pos = u_Projection * u_View * vec4(pos.xyz, 1.0);
    float clip_space_depth = clip_space_pos.z / clip_space_pos.w;
    float linearDepth = (2.0 * 0.1 * 1000.0) / (1000.0 + 0.1 - clip_space_depth * (1000.0 - 0.1));
    return linearDepth / 1000.0;
}

void main() {
    float t = -v_NearPoint.y / (v_FarPoint.y - v_NearPoint.y);

    if (t < 0.0) {
        discard;
    }

    vec3 fragPos3D = v_NearPoint + t * (v_FarPoint - v_NearPoint);

    gl_FragDepth = ComputeDepth(fragPos3D);

    float linearDepth = ComputeLinearDepth(fragPos3D);
    float fading = max(0.0, 1.0 - linearDepth);

    vec4 minorGrid = Grid(fragPos3D, 1.0 / u_GridMinorScale, true) * float(t > 0);
    vec4 majorGrid = Grid(fragPos3D, 1.0 / u_GridScale, false) * float(t > 0);

    vec4 gridColor = mix(minorGrid, majorGrid, majorGrid.a);
    gridColor.a *= fading;

    // Y axis (green) - vertical line at origin
    float distToOrigin = length(fragPos3D.xz);
    if (distToOrigin < u_AxisThickness * 10.0) {
        float axisAlpha = 1.0 - smoothstep(0.0, u_AxisThickness * 10.0, distToOrigin);
        gridColor = mix(gridColor, vec4(0.0, 1.0, 0.0, 1.0), axisAlpha);
    }

    FragColor = gridColor;
}
