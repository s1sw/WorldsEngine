#version 450
#extension GL_EXT_multiview : enable

layout (location = 0) in vec4 inCol;
layout (location = 0) out vec4 FragColor;

void main() {
    FragColor = inCol;
}