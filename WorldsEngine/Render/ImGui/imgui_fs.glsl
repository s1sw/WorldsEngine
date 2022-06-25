#version 450

layout (location = 0) in vec2 in_UV;
layout (location = 1) in vec4 in_Color;

layout (location = 0) out vec4 FragColor;

layout (binding = 0) uniform sampler2D Texture;

void main()
{
    FragColor = in_Color * texture(Texture, in_UV);
}