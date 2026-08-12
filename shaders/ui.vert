#version 330 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in float aUseTex;

out vec4 FragColor;
out vec2 TexCoord;
out float UseTex;

uniform mat4 uProjection;

void main()
{
    FragColor = aColor;
    TexCoord = aTexCoord;
    UseTex = aUseTex;
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
}
