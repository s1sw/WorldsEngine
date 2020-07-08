#version 450

layout(location = 0) in vec3 inPosition;

layout(push_constant) uniform PushConstants {
	mat4 mvp;
};

void main() {
	gl_Position = mvp * vec4(inPosition, 1.0); // Apply MVP transform
	gl_Position.y = -gl_Position.y; // Account for Vulkan viewport weirdness
}