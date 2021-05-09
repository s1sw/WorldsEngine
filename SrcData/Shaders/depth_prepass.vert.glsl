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

layout(binding = 3) readonly buffer ModelMatrices {
    mat4 modelMatrices[1024];
};

layout(push_constant) uniform PushConstants {
    vec4 texScaleOffset;

    int modelMatrixIdx;
    int matIdx;
    int vpIdx;
    uint objectId;

    ivec2 pixelPickCoords;
    // Misc flag uint
    // 32 bits
    // 1 - Activate object picking             (1)
    // 2 - Debug display normals               (2)
    // 3 - Debug display metallic              (4)
    // 4 - Debug display roughness             (8)
    // 5 - Debug display AO                    (16)
    // 6 - Debug display normal map            (32)
    // 7 - Lighting only                       (64)
    // 8 - World space UVs (XY)                (128)
    // 9 - World space UVs (XZ)                (256)
    // 10 - World space UVs (ZY)               (512)
    // 11 - World space UVs (pick with normal) (1024)
    // 12 - Debug display UVs                  (2048)
    // 13 - Use cubemap parallax               (4096)
    uint miscFlag;
    uint cubemapIdx;

    vec3 cubemapExt;
    uint pad;
    vec3 cubemapPos;
    uint pad2;
};

void main() {
    mat4 model = modelMatrices[modelMatrixIdx];
    // On AMD driver 20.10.1 (and possibly earlier) using gl_ViewIndex seems to cause a driver crash
    int vpMatIdx = vpIdx + gl_ViewIndex;

    mat4 projMat = projection[vpMatIdx];

    gl_Position = projMat * view[vpMatIdx] * model * vec4(inPosition, 1.0); // Apply MVP transform

    gl_Position.y = -gl_Position.y; // Account for Vulkan viewport weirdness
    outUV = inUV;
}
