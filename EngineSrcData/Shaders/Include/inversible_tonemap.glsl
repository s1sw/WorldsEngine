#ifndef INVERSIBLE_TONEMAP_H
#define INVERSIBLE_TONEMAP_H

float rcp(float x) { return 1.0 / x; }

float gmax3(float x, float y, float z) {
    return max(x, max(y, z));
}

// Apply this to tonemap linear HDR color "c" after a sample is fetched in the resolve.
// Note "c" 1.0 maps to the expected limit of low-dynamic-range monitor output.
vec3 Tonemap(vec3 c) { return c * rcp(gmax3(c.r, c.g, c.b) + 1.0); }

// When the filter kernel is a weighted sum of fetched colors,
// it is more optimal to fold the weighting into the tonemap operation.
vec3 TonemapWithWeight(vec3 c, float w) { return c * (w * rcp(gmax3(c.r, c.g, c.b) + 1.0)); }

// Apply this to restore the linear HDR color before writing out the result of the resolve.
vec3 TonemapInvert(vec3 c) { return c * rcp(1.0 - gmax3(c.r, c.g, c.b)); }

#endif