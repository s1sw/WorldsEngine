#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_multiview : enable
#define PI 3.1415926535

#ifdef FRAGMENT
layout(early_fragment_tests) in;
layout(location = 0) out vec4 FragColor;
#endif

#ifdef VERTEX
layout(location = 0) in vec3 inPosition;
#endif

const int LT_POINT = 0;
const int LT_SPOT = 1;
const int LT_DIRECTIONAL = 2;

layout(binding = 0) uniform MultiVP {
	mat4 view[8];
	mat4 projection[8];
    vec4 viewPos[8];
};

layout(std140, binding = 3) uniform ModelMatrices {
	mat4 modelMatrices[1024];
};

layout(push_constant) uniform PushConstants {
	vec4 texScaleOffset;
    int modelMatrixIdx;
    int matIdx;
    int vpIdx;
    uint objectId;
    ivec2 pixelPickCoords;
    uint doPicking;
};

#ifdef VERTEX
void main() {
	mat4 model = modelMatrices[modelMatrixIdx];
    int vpMatIdx = vpIdx + gl_ViewIndex; 
    vec4 outWorldPos = (model * vec4(inPosition, 1.0));

    mat4 projMat = projection[vpMatIdx];

    gl_Position = projection[vpMatIdx] * view[vpMatIdx] * outWorldPos; // Apply MVP transform
    gl_Position.y = -gl_Position.y; // Account for Vulkan viewport weirdness
}
#endif

#ifdef FRAGMENT
void main() {
    FragColor = vec4(1.0);
}
#endif