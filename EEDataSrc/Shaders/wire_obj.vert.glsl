#version 450
#extension GL_EXT_multiview : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec2 outUV;

layout(binding = 0) uniform MultiVP {
	mat4 view[8];
	mat4 projection[8];
	vec4 viewPos[8];
};

layout(std140, binding = 1) uniform ModelMatrices {
	mat4 modelMatrices[1024];
};

layout(push_constant) uniform PushConstants {
	vec4 texScaleOffset;
	// (x: model matrix index, y: material index, z: vp index)
	ivec4 ubIndices;
    ivec2 pixelPickCoords;
};

void main() {
    mat4 model = modelMatrices[ubIndices.x];
    gl_Position = projection[ubIndices.z + gl_ViewIndex] * view[ubIndices.z + gl_ViewIndex] * model * vec4(inPosition, 1.0); // Apply MVP transform
    gl_Position.y = -gl_Position.y; // Account for Vulkan viewport weirdness
	outUV = inUV;
}