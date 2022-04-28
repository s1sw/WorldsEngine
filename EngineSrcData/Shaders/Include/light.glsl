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

vec3 getLightDirection(Light l, vec3 worldPos) {
    switch(getLightType(l)) {
    case LT_POINT:
    case LT_SPOT:
    case LT_SPHERE:
    case LT_TUBE:
        return l.pack2.xyz - worldPos;
    case LT_DIRECTIONAL:
        return l.pack1.xyz;
    }
}

struct LightingTile {
    uint lightIdMasks[8];
    uint cubemapIdMasks[2];
    uint aoBoxIdMasks[2];
    uint aoSphereIdMasks[2];

    vec4 frustumPlanes[6];
    vec3 aabbCenter;
    uint minDepthU;
    vec3 aabbExtents;
    uint maxDepthU;
};
#endif
