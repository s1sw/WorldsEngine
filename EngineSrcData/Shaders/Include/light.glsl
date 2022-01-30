#ifndef LIGHT_H
#define LIGHT_H
const uint LT_POINT = 0;
const uint LT_SPOT = 1;
const uint LT_DIRECTIONAL = 2;
const uint LT_SPHERE = 3;
const uint LT_TUBE = 4;

struct Light {
    // (color rgb, type)
    vec4 pack0;
    // (direction xyz or first point for tube, spotlight cutoff, sphere light radius or tube light radius)
    vec4 pack1;
    // (position xyz or second point for tube)
    vec3 pack2;
    float distanceCutoff;
};

uint getLightType(Light l) {
    return floatBitsToUint(l.pack0.w) & 7; // 0b111
}

uint getShadowmapIndex(Light l) {
    return (floatBitsToUint(l.pack0.w) >> 3) & 0xF;
}
#endif
