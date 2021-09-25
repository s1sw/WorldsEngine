#version 450
#extension GL_EXT_multiview : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec2 outUV;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
	uint matIdx;
};

void main() {
    gl_Position = mvp * vec4(inPosition, 1.0); // Apply MVP transform
    gl_Position.y = -gl_Position.y; // Account for Vulkan viewport weirdness
	outUV = inUV;
}
