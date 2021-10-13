#ifndef STANDARD_PUSH_CONSTANTS_H
#define STANDARD_PUSH_CONSTANTS_H
layout(push_constant) uniform PushConstants {
    int modelMatrixIdx;
    int matIdx;
    int vpIdx;
    uint objectId;

    vec3 cubemapExt;
    uint skinningOffset;
    vec3 cubemapPos;
    uint pad2;

    vec4 texScaleOffset;

    ivec2 pixelPickCoords;
    // Misc flag uint
    // 32 bits
    // 1 - Activate object picking             (1)
    // 2 - Debug display normals               (2)
    // 3 - Debug display metallic              (4)
    // 4 - Debug display roughness             (8)
    // 5 - Debug display AO                    (16)
    // 6 - Debug display normal map            (32)
    // 7 - Debug lighting only                 (64)
	// 8 - Debug display UVs                   (128)
	// 9 - Debug display shadowmap cascades    (256)
	// 10 - Debug display albedo               (512)
    // 11 - World space UVs (XY)               (1024)
    // 12 - World space UVs (XZ)               (2048)
    // 13 - World space UVs (ZY)               (4096)
    // 14 - World space UVs (pick with normal) (8192)
    // 15 - Use cubemap parallax               (16384)
    // 16 - Disable shadows                    (32768)
    uint miscFlag;
    uint cubemapIdx;
    //total: 80 bytes
};

const int DBG_FLAG_NORMALS = 2;
const int DBG_FLAG_METALLIC = 4;
const int DBG_FLAG_ROUGHNESS = 8;
const int DBG_FLAG_AO = 16;
const int DBG_FLAG_NORMAL_MAP = 32;
const int DBG_FLAG_LIGHTING_ONLY = 64;
const int DBG_FLAG_UVS = 128;
const int DBG_FLAG_SHADOW_CASCADES = 256;
const int DBG_FLAG_ALBEDO = 512;
const int DBG_FLAG_LIGHT_TILES = 1024;

const int MISC_FLAG_UV_XY = 2048;
const int MISC_FLAG_UV_XZ = 4096;
const int MISC_FLAG_UV_ZY = 8192;
const int MISC_FLAG_UV_PICK = 16384;
const int MISC_FLAG_CUBEMAP_PARALLAX = 32768;
const int MISC_FLAG_DISABLE_SHADOWS = 65536;
#endif
