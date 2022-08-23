import InversibleTonemap;
import SRGB;

RWTexture2D<float4> resultImage;

#ifdef MSAA
    #ifdef MULTIVP
        Sampler2DMSArray hdrImage;
    #else
        Sampler2DMS hdrImage;
    #endif
#else
    #ifdef MULTIVP
        Sampler2DArray hdrImage;
    #else
        Sampler2D hdrImage;
    #endif
#endif

#ifdef MULTIVP
    Sampler2DArray bloomImage;
#else
    Sampler2D bloomImage;
#endif

[[vk::push_constant]]
cbuffer TonemapSettings
{
    int idx;
    float exposure;
    float contrast;
    float saturation;
    float resolutionScale;
};

interface ITonemapper
{
    float3 Tonemap(float3 color);
}

// haha shader oop go brrr

// adapted from http://filmicworlds.com/blog/filmic-tonemapping-operators/
struct Uncharted2Tonemapper : ITonemapper
{
    float3 whiteScale;

    __init()
    {
        static const float W = 11.2;
        whiteScale = 1.0 / TonemapInternal(W);
    }

    float3 TonemapInternal(float3 x)
    {
        static const float A = 0.15;
        static const float B = 0.50;
        static const float C = 0.10;
        static const float D = 0.20;
        static const float E = 0.02;
        static const float F = 0.30;

        return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
    }

    float3 Tonemap(float3 color)
    {
        color *= 16.0;

        float3 curr = TonemapInternal(color);

        return curr * whiteScale;
    }
}

// adapted from https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/ACES.hlsl
struct ACESTonemapper : ITonemapper
{
    __init() {}

    float3 Tonemap(float3 color)
    {
        float3 x = color;

        float3x3 m1 = transpose(float3x3(
            0.59719, 0.07600, 0.02840,
            0.35458, 0.90834, 0.13383,
            0.04823, 0.01566, 0.83777
        ));

        float3x3 m2 = transpose(float3x3(
            1.60475, -0.10208, -0.00327,
            -0.53108,  1.10813, -0.07276,
            -0.07367, -0.00605,  1.07602
        ));

        float3 v = mul(m1, x);

        float3 a = v * (v + 0.0245786) - 0.000090537;
        float3 b = v * (0.983729 * v + 0.4329510) + 0.238081;

        return saturate(mul(m2, a / b));
    }
}

// adapted from https://web.archive.org/web/20170226070045/http://www.vocas.nl/webfm_send/964
struct LogConverter
{
    const static float cut = 0.004160;
    const static float a = 250.0;
    const static float b = -0.928805;
    const static float c = 0.244161;
    const static float d = 0.386036;
    const static float e = 238.584745;
    const static float f = -0.839385;

    static float LinToLog(float x)
    {
        return (x > cut) ? c * log10(a * x + b) + d : e * x + f;
    }

    static float3 LinToLog(float3 x)
    {
        return float3(
            LinToLog(x.x),
            LinToLog(x.y),
            LinToLog(x.z)
        );
    }

    static float LogToLin(float x)
    {
        return (x > e * cut + f) ? (pow(10, (x - d) / c) - b) / a : (x - f) / e;
    }

    static float3 LogToLin(float3 x)
    {
        return float3(
            LogToLin(x.x),
            LogToLin(x.y),
            LogToLin(x.z)
        );
    }
}

struct TonemapTextureSettings
{
    uint msaaSamples;
    uint hdrWidth;
    uint hdrHeight;

    property float2 HDRResolution
    {
        get { return float2(hdrWidth, hdrHeight); }
    }
}

TonemapTextureSettings GetTextureSettings()
{
    TonemapTextureSettings ts;
    uint w, h, arrayCount, samples;
#ifdef MSAA
    #ifdef MULTIVP
        hdrImage.GetDimensions(w, h, arrayCount, samples);
    #else
        hdrImage.GetDimensions(w, h, samples);
    #endif
#else
    #ifdef MULTIVP
        hdrImage.GetDimensions(w, h, arrayCount);
    #else
        hdrImage.GetDimensions(w, h);
    #endif
#endif
    ts.hdrWidth = w;
    ts.hdrHeight = h;
    ts.msaaSamples = samples;

    return ts;
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
    TonemapTextureSettings texSettings = GetTextureSettings();

    float3 accumulator = 0.0;
    float2 uv = (float2(threadID.xy) + 0.5) / (texSettings.HDRResolution / resolutionScale);

#ifdef MULTIVP
    float3 bloom = bloomImage.SampleLevel(float3(uv * resolutionScale, idx), 0).xyz;
#else
    float3 bloom = bloomImage.SampleLevel(uv * resolutionScale, 0).xyz;
#endif
    bloom = max(bloom, float3(0.0));

#ifdef MSAA
    // If we're using MSAA, resolve using the reversible tonemapper
    AMDReversibleTonemapper reversibleTonemap;
    for (int i = 0; i < texSettings.msaaSamples; i++)
    {
#ifdef MULTIVP
        float3 raw = hdrImage.Load(int3(threadID.xy, idx), i).xyz;
#else
        float3 raw = hdrImage.Load(threadID.xy, i).xyz;
#endif

        accumulator += reversibleTonemap.Tonemap(lerp(raw, bloom, 0.2));
    }

    accumulator *= rcp(texSettings.msaaSamples);
    accumulator = reversibleTonemap.TonemapInvert(accumulator);
#else
    // Otherwise just load it directly
#ifdef MULTIVP
    accumulator = hdrImage.Load(int4(threadID.xy, idx, 0)).xyz;
#else
    accumulator = hdrImage.Load(int3(threadID.xy, 0)).xyz;
#endif
#endif

    float3 lumaWeights = float3(0.3, 0.59, 0.11);
    accumulator *= exposure;
    float3 grey = dot(lumaWeights, accumulator);
    accumulator = max(lerp(grey, accumulator, saturation), 0.0);
    // Tonemap
    const bool useACES = true;
    if (useACES)
    {
        ITonemapper tonemapper1 = ACESTonemapper();
        accumulator = LogConverter.LinToLog(accumulator);
        accumulator = ((accumulator - 0.18) * contrast) + 0.18;
        accumulator = LogConverter.LogToLin(accumulator);
        accumulator = saturate(tonemapper1.Tonemap(accumulator));
    }
    else
    {
        ITonemapper tonemapper = Uncharted2Tonemapper();
        accumulator = saturate(tonemapper.Tonemap(accumulator));
    }

    resultImage[threadID.xy] = float4(ConvertToSRGB(accumulator), 1.0);
}