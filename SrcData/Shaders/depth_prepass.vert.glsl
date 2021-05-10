#version 450
#extension GL_EXT_multiview : enable

layout(location = 0) in vec3 inPosition;

// for alpha test
layout(location = 1) in vec2 inUV;
layout(location = 0) out vec2 outUV;

#include <standard_descriptors.glsl>
#include <standard_push_constants.glsl>

void main() {
    mat4 model = modelMatrices[modelMatrixIdx];
    // On AMD driver 20.10.1 (and possibly earlier) using gl_ViewIndex seems to cause a driver crash
    int vpMatIdx = vpIdx + gl_ViewIndex;

    mat4 projMat = projection[vpMatIdx];

    gl_Position = projMat * view[vpMatIdx] * model * vec4(inPosition, 1.0); // Apply MVP transform

    gl_Position.y = -gl_Position.y; // Account for Vulkan viewport weirdness
    outUV = inUV;
}
