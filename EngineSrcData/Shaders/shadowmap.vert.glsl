#version 450
#extension GL_EXT_multiview : enable
#include <material.glsl>

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;
#ifdef SKINNED
layout(location = 2) in vec4 inBoneWeights;
layout(location = 3) in uvec4 inBoneIds;
#endif

layout(location = 0) out vec2 outUV;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
	uint matIdx;
    uint skinningOffset;
};

layout (binding = 0) uniform sampler2D tex2dSampler[];
layout(std140, binding = 1) readonly buffer MaterialSettingsBuffer {
    Material materials[256];
};

#ifdef SKINNED
layout (binding = 2) readonly buffer BoneTransforms {
    mat4 matrices[];
} buf_BoneTransforms;
#endif

void main() {
#ifdef SKINNED
    mat4 model = mat4(0.0f);

    model  = buf_BoneTransforms.matrices[skinningOffset + inBoneIds[0]] * inBoneWeights[0];
    model += buf_BoneTransforms.matrices[skinningOffset + inBoneIds[1]] * inBoneWeights[1];
    model += buf_BoneTransforms.matrices[skinningOffset + inBoneIds[2]] * inBoneWeights[2];
    model += buf_BoneTransforms.matrices[skinningOffset + inBoneIds[3]] * inBoneWeights[3];

    gl_Position = mvp * model * vec4(inPosition, 1.0); // Apply MVP transform
#else
    gl_Position = mvp * vec4(inPosition, 1.0); // Apply MVP transform
#endif

    gl_Position.y = -gl_Position.y; // Account for Vulkan viewport weirdness
	outUV = inUV;
}
