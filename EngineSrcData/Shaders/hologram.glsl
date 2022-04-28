
#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_KHR_shader_subgroup_arithmetic : require
#extension GL_ARB_shader_ballot : require
#define MULTIVIEW
#ifdef MULTIVIEW
#extension GL_EXT_multiview : enable
#endif
#define MAX_SHADOW_LIGHTS 16
#define HIGH_QUALITY_SHADOWS
#include <math.glsl>
#include <light.glsl>
#include <material.glsl>
#include <pbrutil.glsl>
#include <pbrshade.glsl>
#include <parallax.glsl>
#include <shadercomms.glsl>
#include <aobox.glsl>

layout(location = 0) VARYING(vec4, WorldPos);
layout(location = 1) VARYING(vec3, Normal);
layout(location = 2) VARYING(vec4, Tangent);
layout(location = 3) VARYING(vec2, UV);
layout(location = 4) VARYING(flat uint, UvDir);

#ifdef FRAGMENT
//#ifdef EFT
layout(early_fragment_tests) in;
//#endif
layout(location = 0) out vec4 FragColor;

layout(constant_id = 0) const bool ENABLE_PICKING = false;
layout(constant_id = 1) const float PARALLAX_MAX_LAYERS = 32.0;
layout(constant_id = 2) const float PARALLAX_MIN_LAYERS = 4.0;
layout(constant_id = 3) const bool DO_PARALLAX = false;
layout(constant_id = 4) const bool ENABLE_PROXY_AO = true;
#endif

#ifdef VERTEX
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in float inBitangentSign;
layout(location = 4) in vec2 inUV;
#ifdef SKINNED
layout(location = 5) in vec4 inBoneWeights;
layout(location = 6) in uvec4 inBoneIds;
#endif
#endif

#include <standard_descriptors.glsl>
#include <standard_push_constants.glsl>

vec3 getViewPos() {
#ifdef MULTIVIEW
    return viewPos[gl_ViewIndex].xyz;
#else
    return viewPos[0].xyz;
#endif
}

#ifdef VERTEX
void main() {
#ifdef SKINNED
    mat4 model = mat4(0.0f);

    model =
        inBoneWeights[0] * buf_BoneTransforms.matrices[skinningOffset + inBoneIds[0]]  +
        inBoneWeights[1] * buf_BoneTransforms.matrices[skinningOffset + inBoneIds[1]]  +
        inBoneWeights[2] * buf_BoneTransforms.matrices[skinningOffset + inBoneIds[2]]  +
        inBoneWeights[3] * buf_BoneTransforms.matrices[skinningOffset + inBoneIds[3]];

    model = modelMatrices[modelMatrixIdx + gl_InstanceIndex] * model;
#else
    mat4 model = modelMatrices[modelMatrixIdx];
#endif

    int vpMatIdx = vpIdx;

#ifdef MULTIVIEW
    vpMatIdx += gl_ViewIndex;
#endif

    outWorldPos = (model * vec4(inPosition, 1.0));

    gl_Position = (projection[vpMatIdx] * view[vpMatIdx] * model) * vec4(inPosition, 1.0); // Apply MVP transform

    model = transpose(inverse(model));

    outNormal = normalize((model * vec4(inNormal, 0.0)).xyz);
    outTangent = normalize(vec4((model * vec4(inTangent, 0.0)).xyz, inBitangentSign));
    gl_Position.y = -gl_Position.y; // Account for Vulkan viewport weirdness

    vec2 uv = inUV;
    outUvDir = 0;

    if ((miscFlag & MISC_FLAG_UV_XY) == MISC_FLAG_UV_XY) {
        uv = outWorldPos.xy;
        outUvDir = 1;
    } else if ((miscFlag & MISC_FLAG_UV_XZ) == MISC_FLAG_UV_XZ) {
        uv = outWorldPos.xz;
        outUvDir = 2;
    } else if ((miscFlag & MISC_FLAG_UV_ZY) == MISC_FLAG_UV_ZY) {
        uv = outWorldPos.zy;
        outUvDir = 3;
    } else if ((miscFlag & MISC_FLAG_UV_PICK) == MISC_FLAG_UV_PICK) {
        // Find maximum axis
        uint maxAxis = 0;

        vec3 dots = vec3(0.0);

        dots.x = abs(dot(outNormal, vec3(1.0, 0.0, 0.0)));
        dots.y = abs(dot(outNormal, vec3(0.0, 1.0, 0.0)));
        dots.z = abs(dot(outNormal, vec3(0.0, 0.0, 1.0)));
        float maxProduct = max(dots.x, max(dots.y, dots.z));

        // Assume flat surface for tangents
        if (dots.x == maxProduct) {
            uv = outWorldPos.zy;
            outUvDir = 1;
        } else if (dots.y == maxProduct) {
            uv = outWorldPos.xz;
            outUvDir = 2;
        } else {
            uv = outWorldPos.xy;
            outUvDir = 3;
        }
    }

    outUV = (uv * texScaleOffset.xy) + texScaleOffset.zw;
}
#endif

#ifdef FRAGMENT
void main() {
    float dp = 1.0 - dot(inNormal, normalize(getViewPos() - inWorldPos.xyz));
    dp *= dp;
    vec3 col = pow(vec3(0.0, 0.25, 0.75) * 5, vec3(2.2)) * max(dp, 0.05);
    FragColor = vec4(col, 0.5);
}
#endif
