#version 450
#extension GL_EXT_multiview : enable

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec4 inCol;

layout (location = 0) out vec4 outCol;


layout(binding = 0) uniform MultiVP {
	mat4 view[4];
	mat4 projection[4];
    vec4 viewPos[4];
};

void main() {
    uint vpIdx = 0;
    #ifndef AMD_VIEWINDEX_WORKAROUND
    vpIdx += gl_ViewIndex;
    #endif
    gl_Position = projection[vpIdx] * view[vpIdx] * vec4(inPos, 1.0);
    outCol = inCol;
    gl_Position.y = -gl_Position.y;
}