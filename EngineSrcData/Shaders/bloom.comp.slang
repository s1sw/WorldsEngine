#ifdef MULTIVP
Sampler2DArray<float4> InputTexture;
RWTexture2DArray<float4> OutputTexture;
#else
Sampler2D<float4> InputTexture;
RWTexture2D<float4> OutputTexture;
#endif

[[vk::push_constant]]
cbuffer PC
{
    uint inputMipLevel;
    uint inputArrayIdx;

    uint2 outputResolution;
}

static int arrayIdx = 0;

float4 samp(float2 uv)
{
#ifdef MULTIVP
    return InputTexture.SampleLevel(float3(uv, arrayIdx), inputMipLevel);
#else
    return InputTexture.SampleLevel(uv, inputMipLevel);
#endif
}

#include "BloomFilters.slang"

float4 DoDownsample(float2 uv, float2 resolution)
{
    return Downsample13(uv, resolution);
}

float4 DoUpsample(float2 uv, float2 resolution)
{
#ifdef MULTIVP
    float4 orig = InputTexture.SampleLevel(float3(uv, arrayIdx), inputMipLevel - 1);
#else
    float4 orig = InputTexture.SampleLevel(uv, inputMipLevel - 1);
#endif
    float4 blurred = UpsampleTent(uv, resolution);

    return lerp(orig, blurred, 0.5);
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
    arrayIdx = int(threadID.z);
    float2 resolution = outputResolution; 
    float2 uv = (float2(threadID.xy) + 0.5) / resolution;

#ifdef UPSAMPLE
    float4 result = DoUpsample(uv, resolution);
#else
    float4 result = DoDownsample(uv, resolution);
#endif


#ifdef MULTIVP
    OutputTexture[int3(threadID.xy, threadID.z)] = result;
#else
    OutputTexture[threadID.xy] = result;
#endif
}