#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_KHR_shader_subgroup_arithmetic : require
#extension GL_ARB_shader_ballot : require
#define MULTIVIEW
#ifdef MULTIVIEW
#extension GL_EXT_multiview : enable
#endif
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

layout(early_fragment_tests) in;
layout(location = 0) out vec4 FragColor;

layout(constant_id = 0) const bool ENABLE_PICKING = false;
layout(constant_id = 1) const float PARALLAX_MAX_LAYERS = 32.0;
layout(constant_id = 2) const float PARALLAX_MIN_LAYERS = 4.0;
layout(constant_id = 3) const bool DO_PARALLAX = false;
layout(constant_id = 4) const bool ENABLE_PROXY_AO = true;

#include <standard_descriptors.glsl>
#include <standard_push_constants.glsl>

vec3 getViewPos() {
#ifdef MULTIVIEW
    return viewPos[gl_ViewIndex].xyz;
#else
    return viewPos[0].xyz;
#endif
}

void main() {
    float dp = 1.0 - dot(inNormal, normalize(getViewPos() - inWorldPos.xyz));
    dp *= dp;
    vec3 col = pow(vec3(0.0, 0.25, 0.75) * 5, vec3(2.2)) * max(dp, 0.05);
    FragColor = vec4(col, 0.5);
}
