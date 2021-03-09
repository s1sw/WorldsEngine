#version 450

layout(location = 0) in vec3 inPosition;

layout(push_constant) uniform PushConstants {
	mat4 vp;
	mat4 model;
};

void main() {
	gl_Position = vp * model * vec4(inPosition, 1.0); // Apply MVP transform
	gl_Position.y = -gl_Position.y; // Account for Vulkan viewport weirdness
}