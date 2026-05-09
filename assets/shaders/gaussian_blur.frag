in vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_Image;
uniform int u_Horizontal;

// 9-tap Gaussian kernel (center + 4 symmetric samples per axis).
const float kWeights[5] = float[](0.2270270270, 0.1945945946, 0.1216216216, 0.0540540541, 0.0162162162);

void main()
{
    vec2 texelSize = 1.0 / vec2(textureSize(u_Image, 0));
    vec3 result = texture(u_Image, v_TexCoord).rgb * kWeights[0];

    for (int i = 1; i < 5; ++i)
    {
        vec2 offset = (u_Horizontal == 1)
                          ? vec2(texelSize.x * float(i), 0.0)
                          : vec2(0.0, texelSize.y * float(i));

        result += texture(u_Image, v_TexCoord + offset).rgb * kWeights[i];
        result += texture(u_Image, v_TexCoord - offset).rgb * kWeights[i];
    }

    FragColor = vec4(result, 1.0);
}