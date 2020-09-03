#version 450
#extension GL_EXT_multiview : enable

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec4 inCol;

layout (location = 0) out vec4 outCol;


layout(binding = 0) uniform MultiVP {
	mat4 view[8];
	mat4 projection[8];
    vec4 viewPos[8];
};

void main() {
    gl_Position = projection[gl_ViewIndex] * view[gl_ViewIndex] * vec4(inPos, 1.0);
    outCol = inCol;
    gl_Position.y = -gl_Position.y;
}