float gmax3(float x, float y, float z)
{
    return max(x, max(y, z));
}

// adapted from https://gpuopen.com/learn/optimized-reversible-tonemapper-for-resolve/
struct AMDReversibleTonemapper
{
    float3 Tonemap(float3 color)
    {
        return color * rcp(gmax3(color.r, color.g, color.b) + 1.0);
    }
    
    float3 TonemapInvert(float3 color)
    {
        return color * rcp(1.0 - gmax3(color.r, color.g, color.b));
    }

    float3 TonemapWithWeight(float3 color, float w)
    {
        return color * (w * rcp(gmax3(color.r, color.g, color.b) + 1.0));
    }
}
