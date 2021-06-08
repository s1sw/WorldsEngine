#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#include <material.glsl>

layout (location = 0) out vec4 FragColor;

layout (location = 0) in vec2 inUV;

#include <standard_descriptors.glsl>
#include <standard_push_constants.glsl>

void main() {
    Material mat = materials[matIdx];
    vec2 uv = (inUV * texScaleOffset.xy) + texScaleOffset.zw;
    vec3 invCol = 1.0 - texture(tex2dSampler[mat.albedoTexIdx], uv).xyz;
    FragColor = vec4(invCol, 1.0);
}
