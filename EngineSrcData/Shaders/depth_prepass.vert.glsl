#version 450
#extension GL_EXT_multiview : enable

layout(location = 0) in vec3 inPosition;

// for alpha test
layout(location = 1) in vec2 inUV;
#ifdef SKINNED
layout(location = 2) in vec4 inBoneWeights;
layout(location = 3) in uvec4 inBoneIds;
#endif

layout(location = 0) out vec2 outUV;

#include <standard_descriptors.glsl>
#include <standard_push_constants.glsl>

void main() {
#ifdef SKINNED
    mat4 model = mat4(0.0f);

    //for (int i = 0; i < 4; i++) {
    //    model += buf_BoneTransforms.matrices[skinningOffset + inBoneIds[i]] * inBoneWeights[i];
    //}
    model  = buf_BoneTransforms.matrices[skinningOffset + inBoneIds[0]] * inBoneWeights[0];
    model += buf_BoneTransforms.matrices[skinningOffset + inBoneIds[1]] * inBoneWeights[1];
    model += buf_BoneTransforms.matrices[skinningOffset + inBoneIds[2]] * inBoneWeights[2];
    model += buf_BoneTransforms.matrices[skinningOffset + inBoneIds[3]] * inBoneWeights[3];
#else
    mat4 model = modelMatrices[modelMatrixIdx + gl_InstanceIndex];
#endif
    int vpMatIdx = vpIdx + gl_ViewIndex;

    mat4 projMat = projection[vpMatIdx];

    gl_Position = projMat * view[vpMatIdx] * model * vec4(inPosition, 1.0); // Apply MVP transform

    gl_Position.y = -gl_Position.y; // Account for Vulkan viewport weirdness
    outUV = inUV;
}
