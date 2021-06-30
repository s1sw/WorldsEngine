#ifndef LIGHT_H
#define LIGHT_H
const int LT_POINT = 0;
const int LT_SPOT = 1;
const int LT_DIRECTIONAL = 2;
const int LT_SPHERE = 3;
const int LT_TUBE = 4;

struct Light {
    // (color rgb, type)
    vec4 pack0;
    // (direction xyz or first point for tube, spotlight cutoff, sphere light radius or tube light radius)
    vec4 pack1;
    // (position xyz or second point for tube, shadow index)
    vec3 pack2;
    uint shadowIdx;
    float distanceCutoff;
    float pad0;
    float pad1;
    float pad2;
};
#endif
