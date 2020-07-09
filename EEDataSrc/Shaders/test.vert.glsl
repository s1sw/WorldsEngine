#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec2 inUV;
layout(location = 4) in float inAO;

layout(location = 0) out vec4 outWorldPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outTangent;
layout(location = 3) out vec2 outUV;
layout(location = 4) out float outAO;
layout(location = 5) out vec4 outShadowPos;

layout(binding = 0) uniform VP {
	mat4 view;
	mat4 projection;
};

struct Light {
	// (color rgb, type)
	vec4 pack0;
	// (direction xyz, spotlight cutoff)
	vec4 pack1;
	// (position xyz, unused)
	vec4 pack2;
};

layout(std140, binding = 1) uniform LightBuffer {
	// (light count, yzw unused)
	vec4 pack0;
	mat4 shadowmapMatrix;
	Light lights[16];
};

layout(std140, binding = 3) uniform ModelMatrices {
	mat4 modelMatrices[1024];
};

layout(push_constant) uniform PushConstants {
	vec4 viewPos;
	vec4 texScaleOffset;
	// (x: model matrix index, y: material index)
	ivec4 ubIndices;
};

void main() {
	mat4 model = modelMatrices[ubIndices.x];
    gl_Position = projection * view * model * vec4(inPosition, 1.0); // Apply MVP transform
	
    outUV = inUV;
    outNormal = normalize(model * vec4(inNormal, 0.0)).xyz;
    outTangent = normalize(model * vec4(inTangent, 0.0)).xyz;
    outWorldPos = (model * vec4(inPosition, 1.0));
	outAO = inAO;
	outShadowPos = shadowmapMatrix * model * vec4(inPosition, 1.0);
    gl_Position.y = -gl_Position.y; // Account for Vulkan viewport weirdness
}