#ifndef LIGHT_H
#define LIGHT_H
const int LT_POINT = 0;
const int LT_SPOT = 1;
const int LT_DIRECTIONAL = 2;

struct Light {
    // (color rgb, type)
    vec4 pack0;
    // (direction xyz, spotlight cutoff)
    vec4 pack1;
    // (position xyz, shadow index)
    vec4 pack2;
};
#endif
