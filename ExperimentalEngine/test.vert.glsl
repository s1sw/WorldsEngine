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

layout(binding = 0) uniform VP {
	mat4 view;
	mat4 projection;
};

layout(push_constant) uniform PushConstants {
	vec4 viewPos;
	mat4 model;
};

void main() {
    gl_Position = projection * view * model * vec4(inPosition, 1.0); // Apply MVP transform
    outUV = inUV;
    outNormal = normalize(model * vec4(inNormal, 0.0)).xyz;
    outTangent = normalize(model * vec4(inTangent, 0.0)).xyz;
    outWorldPos = (model * vec4(inPosition, 1.0));
	outAO = inAO;
    gl_Position.y = -gl_Position.y; // Account for Vulkan viewport weirdness
}