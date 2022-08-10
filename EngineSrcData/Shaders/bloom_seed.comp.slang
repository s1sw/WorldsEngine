#ifdef MSAA
Sampler2DMS<float4> InputTexture;
#else
Sampler2D<float4> InputTexture;
#endif

RWTexture2D<float4> OutputTexture;

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
    #ifndef MSAA
    OutputTexture[threadID.xy] = InputTexture.Load(int3(threadID.xy, 0));
    #else
    OutputTexture[threadID.xy] = InputTexture.Load(threadID.xy, 0);
    #endif
}