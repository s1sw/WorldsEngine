#ifndef STANDARD_DESCRIPTORS_H
#define STANDARD_DESCRIPTORS_H
#include <aobox.glsl>
#include <light.glsl>
#include <material.glsl>

layout(binding = 0) uniform MultiVP {
    mat4 view[2];
    mat4 projection[2];
    vec4 viewPos[2];
};

layout(std140, binding = 1) uniform LightBuffer {
    // (light count, yzw cascade texels per unit)
    vec4 pack0;
    // (ao box count, yzw unused)
    vec4 pack1;
    mat4 dirShadowMatrices[3];
    AOBox aoBox[16];
    Light lights[128];
};

layout(std140, binding = 2) readonly buffer MaterialSettingsBuffer {
    Material materials[256];
};

layout(std140, binding = 3) readonly buffer ModelMatrices {
    mat4 modelMatrices[1024];
};

layout (binding = 4) uniform sampler2D tex2dSampler[];
layout (binding = 5) uniform sampler2DArrayShadow shadowSampler;
layout (binding = 6) uniform samplerCube cubemapSampler[];
layout (binding = 7) uniform sampler2D brdfLutSampler;

layout(std430, binding = 8) writeonly buffer PickingBuffer {
    uint objectID;
} pickBuf;
#endif
