#version 450
#extension GL_EXT_multiview : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec2 outUV;

layout(binding = 0) uniform MultiVP {
	mat4 view[4];
	mat4 projection[4];
	vec4 viewPos[4];
};

layout(std140, binding = 3) uniform ModelMatrices {
	mat4 modelMatrices[512];
};

layout(push_constant) uniform PushConstants {
	vec4 texScaleOffset;
	// (x: model matrix index, y: material index, z: vp index)
	ivec4 ubIndices;
    ivec2 pixelPickCoords;
};

void main() {
    mat4 model = modelMatrices[ubIndices.x];
	uint vpIdx = ubIndices.z;
	#ifndef AMD_VIEWINDEX_WORKAROUND
	vpIdx += gl_ViewIndex;
	#endif
    gl_Position = projection[vpIdx] * view[vpIdx] * model * vec4(inPosition, 1.0); // Apply MVP transform
    gl_Position.y = -gl_Position.y; // Account for Vulkan viewport weirdness
	outUV = inUV;
}