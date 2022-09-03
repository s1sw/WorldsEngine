#ifdef MSAA
#ifdef MULTIVP
    Sampler2DMSArray<float4> InputTexture;
#else
    Sampler2DMS<float4> InputTexture;
#endif
#else
Sampler2D<float4> InputTexture;
#endif

#ifdef MULTIVP
RWTexture2DArray<float4> OutputTexture;
#else
RWTexture2D<float4> OutputTexture;
#endif

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
    #ifndef MSAA
    OutputTexture[int2(threadID.xy)] = InputTexture.Load(int3(int2(threadID.xy), 0));
    #else
        #ifdef MULTIVP
            OutputTexture[int3(threadID.xyz)] = InputTexture.Load(int3(threadID.xyz), 0);
        #else
            OutputTexture[int2(threadID.xy)] = InputTexture.Load(int2(threadID.xy), 0);
        #endif
    #endif
}