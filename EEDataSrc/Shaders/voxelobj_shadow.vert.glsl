#version 450

layout(location = 0) in uvec3 inPosition;

layout(push_constant) uniform PushConstants {
	mat4 vp;
	vec3 chunkOffset;
};


void main() {
	vec3 offsetVertPos = vec3(inPosition) + chunkOffset;
    gl_Position = vp * vec4(offsetVertPos, 1.0); // Apply MVP transform
    gl_Position.y = -gl_Position.y; // Account for Vulkan viewport weirdness
}