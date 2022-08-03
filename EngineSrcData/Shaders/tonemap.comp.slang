RWTexture2D<float4> resultImage;

#ifdef MSAA
Sampler2DMS hdrImage;
#else
Sampler2D hdrImage;
#endif

//Sampler2DArray bloomImage;

[[vk::push_constant]]
cbuffer TonemapSettings
{
    int idx;
    float exposureBias;
    float vignetteRadius;
    float vignetteSoftness;
    float3 vignetteColor;
    float resolutionScale;
};

interface ITonemapper
{
    float3 Tonemap(float3 color, float exposure);
}

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

    float3 Tonemap(float3 color, float exposure)
    {
        color *= 16.0;

        float3 curr = TonemapInternal(exposure * color);

        return curr * whiteScale;
    }
}

struct ACESTonemapper : ITonemapper
{
    __init() {}

    float3 Tonemap(float3 color, float exposure)
    {
        float3 x = color * exposure;
        float a = 2.51f;
        float b = 0.03f;
        float c = 2.43f;
        float d = 0.59f;
        float e = 0.14f;
        return saturate((x*(a*x+b))/(x*(c*x+d)+e));
    }
}

float gmax3(float x, float y, float z)
{
    return max(x, max(y, z));
}

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
    hdrImage.GetDimensions(w, h, samples);
#else
    hdrImage.GetDimensions(w, h);
#endif
    ts.hdrWidth = w;
    ts.hdrHeight = h;
    ts.msaaSamples = samples;

    return ts;
}

float3 ConvertToSRGB(float3 col)
{
    bool3 cutoff = col > 0.0031308;

    float3 high = 1.055 * (pow(col, float3(1.0 / 2.4))) - 0.055;
    float3 low = 12.92 * col;

    return lerp(low, high, cutoff);
}


[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
    TonemapTextureSettings texSettings = GetTextureSettings();

    float3 accumulator = 0.0;
    float2 uv = (float2(threadID.xy) + 0.5) / (texSettings.HDRResolution / resolutionScale);

    //float3 bloom = bloomImage.Sample(float3(uv * resolutionScale, idx)).xyz;
    //bloom = max(bloom, float3(0.0));
    float3 bloom = 0.0;

#ifdef MSAA
    // If we're using MSAA, resolve using the reversible tonemapper
    AMDReversibleTonemapper reversibleTonemap;
    for (int i = 0; i < texSettings.msaaSamples; i++)
    {
        float3 raw = hdrImage.Load(threadID.xy, i).xyz;

        accumulator += reversibleTonemap.Tonemap(lerp(raw, bloom, 0.2));
    }

    accumulator *= rcp(texSettings.msaaSamples);
    accumulator = reversibleTonemap.TonemapInvert(accumulator);
#else
    // Otherwise just load it directly
    accumulator = hdrImage.Load(int3(threadID.xy, 0)).xyz;
#endif

    // Tonemap
    ITonemapper tonemapper = Uncharted2Tonemapper();
    accumulator = saturate(tonemapper.Tonemap(accumulator, exposureBias));

    // Apply circular vignette
    float2 centerVec = (uv - float2(0.5, 0.5));
    float smoothness = 0.2;
    float vig = smoothstep(vignetteRadius, vignetteRadius - vignetteSoftness, length(centerVec));
    float vigBlend = smoothstep(vignetteRadius * 2, vignetteRadius * 2 - (vignetteSoftness * 5), length(centerVec));

    float3 final = pow(
        lerp(accumulator, lerp(float3(0.5, 0.0, 0.0), accumulator, vigBlend), 1.0 - vig),
        1.0 / 2.2
    );

    resultImage[threadID.xy] = float4(ConvertToSRGB(accumulator), 1.0);
}