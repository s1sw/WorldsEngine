Sampler2D<float4> InputTexture;
RWTexture2D<float4> OutputTexture;

[[vk::push_constant]]
cbuffer PC
{
    uint inputMipLevel;
    uint pad;

    uint2 outputResolution;
}

float4 samp(float2 uv)
{
    return InputTexture.SampleLevel(uv, inputMipLevel);
}

#include "BloomFilters.slang"

float4 DoDownsample(float2 uv, float2 resolution)
{
    return Downsample13(uv, resolution);
}

float4 DoUpsample(float2 uv, float2 resolution)
{
    float4 orig = InputTexture.SampleLevel(uv, inputMipLevel - 1);
    float4 blurred = UpsampleTent(uv, resolution);

    return lerp(orig, blurred, 0.5);
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
    float2 resolution = outputResolution; 
    float2 uv = (float2(threadID.xy) + 0.5) / resolution;

#ifdef UPSAMPLE
    float4 result = DoUpsample(uv, resolution);
#else
    float4 result = DoDownsample(uv, resolution);
#endif

    OutputTexture[threadID.xy] = result;
}