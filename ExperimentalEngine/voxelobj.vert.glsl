#version 450

layout(location = 0) in uvec3 inPosition;
layout(location = 1) in uvec4 inPackedNormCorner;
layout(location = 2) in float inAO;
layout(location = 3) in uvec2 inTexID;

layout(location = 0) out vec4 outWorldPos;
layout(location = 1) out flat vec3 outNormal;
layout(location = 2) out vec2 outUV;
layout(location = 3) out float outAO;
layout(location = 4) out flat uvec2 outTexID;
layout(location = 5) out vec4 outLastCSPos;
layout(location = 6) out vec4 outCSPos;

layout(binding = 0) uniform VP {
	mat4 view;
	mat4 projection;
	mat4 viewLast;
	mat4 projLast;
};

layout(push_constant) uniform PushConstants {
	vec4 viewPos;
	vec3 chunkOffset;
};

vec3 faceNormals[6] = vec3[](
	vec3(  0.0,  1.0,  0.0 ), // 0 - Up
	vec3(  0.0, -1.0,  0.0 ), // 1 - Down
	vec3(  0.0,  0.0,  1.0 ), // 2 - Forward
	vec3(  0.0,  0.0, -1.0 ), // 3 - Backward
	vec3(  1.0,  0.0,  0.0 ), // 4 - Right
	vec3( -1.0,  0.0,  0.0 )  // 5 - Left
);

vec2 cornerUVs[4] = vec2[](
	vec2 (0.0, 1.0), // 0
	vec2 (1.0, 1.0), // 1
	vec2 (1.0, 0.0), // 2
	vec2 (0.0, 0.0)  // 3
);

void main() {
	vec3 offsetVertPos = vec3(inPosition) + chunkOffset;
    gl_Position = projection * view * vec4(offsetVertPos, 1.0); // Apply MVP transform
    outUV = cornerUVs[int(inPackedNormCorner.y)];
    outNormal = normalize(faceNormals[int(inPackedNormCorner.x)]);
    outWorldPos = vec4(offsetVertPos, 1.0);
	outAO = inAO;
	outTexID = inTexID;
	outLastCSPos = (projLast * viewLast * vec4(offsetVertPos, 1.0));
	outLastCSPos.y = -outLastCSPos.y;
    gl_Position.y = -gl_Position.y; // Account for Vulkan viewport weirdness
	outCSPos = gl_Position;
	
}