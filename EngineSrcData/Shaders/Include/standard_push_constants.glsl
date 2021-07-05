#ifndef STANDARD_PUSH_CONSTANTS_H
#define STANDARD_PUSH_CONSTANTS_H
layout(push_constant) uniform PushConstants {
    int modelMatrixIdx;
    int matIdx;
    int vpIdx;
    uint objectId;

    vec3 cubemapExt;
    uint pad;
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
    // 7 - Lighting only                       (64)
    // 8 - World space UVs (XY)                (128)
    // 9 - World space UVs (XZ)                (256)
    // 10 - World space UVs (ZY)               (512)
    // 11 - World space UVs (pick with normal) (1024)
    // 12 - Debug display UVs                  (2048)
    // 13 - Use cubemap parallax               (4096)
    // 14 - Debug display shadowmap cascades   (8192)
    // 15 - Disable shadows                    (16384)
    // 16 - Debug display albedo               (32768)
    uint miscFlag;
    uint cubemapIdx;
    //total: 80 bytes
};
#endif
