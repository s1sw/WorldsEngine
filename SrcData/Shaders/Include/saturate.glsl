#ifndef SATURATE_H
#define SATURATE_H
// glsl port of hlsl's saturate
float saturate(float x) {
    return clamp(x, 0.0, 1.0);
}

vec3 saturate(vec3 x) {
    return clamp(x, vec3(0.0), vec3(1.0));
}
#endif
