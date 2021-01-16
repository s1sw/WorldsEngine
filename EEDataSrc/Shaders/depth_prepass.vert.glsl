#version 450
#extension GL_EXT_multiview : enable

layout(location = 0) in vec3 inPosition;

// for alpha test
layout(location = 1) in vec2 inUV;
layout(location = 0) out vec2 outUV;

layout(binding = 0) uniform MultiVP {
	mat4 view[4];
	mat4 projection[4];
    vec4 viewPos[4];
};

layout(binding = 3) uniform ModelMatrices {
	mat4 modelMatrices[1024];
};

layout(push_constant) uniform PushConstants {
	vec4 texScaleOffset; //   16
    int modelMatrixIdx;  // + 4
    int matIdx;          // + 4
    int vpIdx;           // + 4
    uint objectId;       // + 4
    ivec2 pixelPickCoords;//+ 8
    uint doPicking;      // + 4
                         // = 44 bytes out of 128
};

void main() {
	mat4 model = modelMatrices[modelMatrixIdx];
    // On AMD driver 20.10.1 (and possibly earlier) using gl_ViewIndex seems to cause a driver crash
    int vpMatIdx = vpIdx + gl_ViewIndex; 

    #ifndef AMD_VIEWINDEX_WORKAROUND
    vpMatIdx += gl_ViewIndex;
    #endif
    mat4 projMat = projection[vpMatIdx];

    gl_Position = projMat * view[vpMatIdx] * (model * vec4(inPosition, 1.0)); // Apply MVP transform

    gl_Position.y = -gl_Position.y; // Account for Vulkan viewport weirdness
    outUV = inUV;
}
