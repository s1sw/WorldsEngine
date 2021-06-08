#version 450
#extension GL_EXT_multiview : enable

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec4 inCol;

layout (location = 0) out vec4 outCol;


layout(binding = 0) uniform MultiVP {
    mat4 view[2];
    mat4 projection[2];
    vec4 viewPos[2];
};

void main() {
    uint vpIdx = gl_ViewIndex;
    gl_Position = projection[vpIdx] * view[vpIdx] * vec4(inPos, 1.0);
    outCol = inCol;
    gl_Position.y = -gl_Position.y;
}
