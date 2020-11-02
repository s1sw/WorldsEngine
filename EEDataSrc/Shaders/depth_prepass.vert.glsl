#version 450
#extension GL_EXT_multiview : enable

layout(location = 0) in vec3 inPosition;

layout(binding = 0) uniform MultiVP {
	mat4 view[4];
	mat4 projection[4];
    vec4 viewPos[4];
};

layout(binding = 3) uniform ModelMatrices {
	mat4 modelMatrices[512];
};

layout(push_constant) uniform PushConstants {
	vec4 texScaleOffset; //   16
    int modelMatrixIdx;  // + 4
    int matIdx;          // + 4
    int vpIdx;           // + 4
    uint objectId;       // + 4
    ivec2 pixelPickCoords;//+ 8
                        // = 40
};

void main() {
	mat4 model = modelMatrices[modelMatrixIdx];
    // On AMD driver 20.10.1 (and possibly earlier) using gl_ViewIndex seems to cause a driver crash
    int vpMatIdx = vpIdx; // + gl_ViewIndex; 

    #ifndef AMD_VIEWINDEX_WORKAROUND
    vpIdx += gl_ViewIndex;
    #endif
    mat4 projMat = projection[vpMatIdx];

    gl_Position = projMat * view[vpMatIdx] * (model * vec4(inPosition, 1.0)); // Apply MVP transform

    gl_Position.y = -gl_Position.y; // Account for Vulkan viewport weirdness
}