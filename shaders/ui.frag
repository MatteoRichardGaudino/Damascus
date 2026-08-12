#version 330 core

in vec4 FragColor;
in vec2 TexCoord;
in float UseTex;

out vec4 FinalColor;

uniform sampler2D uTexture;

void main()
{
    if (UseTex > 0.5) {
        vec4 texColor = texture(uTexture, TexCoord);
        FinalColor = vec4(FragColor.rgb, FragColor.a * texColor.a);
    } else {
        FinalColor = FragColor;
    }
}
