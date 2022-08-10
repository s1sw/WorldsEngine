static const float PI = 3.1415926535;
static const float PI_HALF = 1.5707963267948966192313216916398;

// [Eberly2014] GPGPU Programming for Games and Science
float fastAcos(float x)
{
#if 1
    float res = -0.156583 * abs(x) + PI_HALF;
    res *= sqrt(1.0 - abs(x));
    return x >= 0 ? res : PI - res;
#else
    return acos(x);
#endif
}

// https://github.com/michaldrobot/ShaderFastLibs/blob/master/ShaderFastMathLib.h
float fastAtan(float inX)
{
	float  x = inX;
	return x*(-0.1784f * abs(x) - 0.0663f * x * x + 1.0301f);
}

// Clamps input to prevent NaNs.
float safeFastAcos(float x)
{
    return fastAcos(clamp(x, -1.0, 1.0));
}

float3x3 adjoint(float4x4 m)
{
    return float3x3(cross(m[1].xyz, m[2].xyz), 
                    cross(m[2].xyz, m[0].xyz), 
                    cross(m[0].xyz, m[1].xyz));
}

float3 parallaxCorrectCubemap(float3 v, float3 center, float3 ma, float3 mi, float3 pos)
{
    float3 dir = normalize(v);
    float3 rbmax = (ma - pos) / dir;
    float3 rbmin = (mi - pos) / dir;

    float3 rbminmax = max(rbmax, rbmin);

    float dist = min(min(rbminmax.x, rbminmax.y), rbminmax.z);

    float3 intersection = pos + dir * dist;

    return intersection - center;
}

float pow5(float a)
{
    return a * a * a * a * a;
}