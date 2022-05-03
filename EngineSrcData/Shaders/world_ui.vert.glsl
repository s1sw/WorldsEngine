#version 450
#extension GL_EXT_multiview : enable

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUv;

layout(location = 0) out vec2 outUv;

layout(binding = 0) uniform MultiVP {
    mat4 view[2];
    mat4 projection[2];
    mat4 inverseVP[2];
    vec4 viewPos[2];
};

layout (push_constant) uniform PC {
    mat4 model;
    uint textureIndex;
};

void main() {
    vec4 projectedPos = projection[gl_ViewIndex] * view[gl_ViewIndex] * model * vec4(inPos, 1.0);
    gl_Position = projectedPos;
    gl_Position.y = -gl_Position.y;

    outUv = inUv;
}
