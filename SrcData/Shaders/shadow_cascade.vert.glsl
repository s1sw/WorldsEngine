#version 450
#extension GL_EXT_multiview : enable

layout(location = 0) in vec3 inPosition;
layout(binding = 0) uniform CascadeMatrices {
    mat4 vpMats[3];
};

layout(push_constant) uniform PushConstants {
    mat4 model;
};

void main() {
    mat4 vp = vpMats[gl_ViewIndex];
    gl_Position = vp * model * vec4(inPosition, 1.0); // Apply MVP transform
    gl_Position.y = -gl_Position.y; // Account for Vulkan viewport weirdness
}
