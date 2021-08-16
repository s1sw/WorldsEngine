#ifndef MATH_H
#define MATH_H
#define PI 3.1415926535
#define PI_HALF 1.5707963267948966192313216916398

// glsl port of hlsl's saturate
float saturate(float x) {
    return clamp(x, 0.0, 1.0);
}

vec3 saturate(vec3 x) {
    return clamp(x, vec3(0.0), vec3(1.0));
}

// [Eberly2014] GPGPU Programming for Games and Science
float fastAcos(float x) {
#if 1
    float res = -0.156583 * abs(x) + PI_HALF;
    res *= sqrt(1.0 - abs(x));
    return x >= 0 ? res : PI - res;
#else
    return acos(x);
#endif
}

// https://github.com/michaldrobot/ShaderFastLibs/blob/master/ShaderFastMathLib.h
float fastAtan(float inX) {
	float  x = inX;
	return x*(-0.1784f * abs(x) - 0.0663f * x * x + 1.0301f);
}

// Clamps input to prevent NaNs.
float safeAcos(float x) {
    return fastAcos(clamp(x, -1.0, 1.0));
}
#endif
