#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout (location = 0) in vec2 in_UV;
layout (location = 1) in vec4 in_Color;

layout (location = 0) out vec4 FragColor;

layout (binding = 0) uniform sampler2D Textures[];

layout (push_constant) uniform PC
{
    vec2 Scale;
    vec2 Translate;
    uint TextureID;
};

void main()
{
    FragColor = in_Color * texture(Textures[TextureID], in_UV);
}