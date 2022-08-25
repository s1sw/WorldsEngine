import Math;
import SRGB;

[[vk::push_constant]]
cbuffer PC
{
    float roughness;
    int faceIdx;
    int outputIsSRGB;
}

SamplerCube<float4> fullCubemap;
RWTexture2D<float4> outFace;

// Adapted from http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
float VdCRadicalInverse(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

float2 Hammersley2D(uint i, uint N)
{
    return float2(float(i) / float(N), VdCRadicalInverse(i));
}

float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
    float alpha = roughness * roughness;
    float alphaSq = alpha * alpha;

    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (alphaSq - 1.0) * Xi.y));
    // and here i was thinking trig identities wouldn't be useful
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta); 

    float3 H;
    H.x = sinTheta * cos(phi);
    H.y = sinTheta * sin(phi);
    H.z = cosTheta;

    float3 upVec = abs(N.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 tangentX = normalize(cross(upVec, N));
    float3 tangentY = cross(N, tangentX);

    return tangentX * H.x + tangentY * H.y + N * H.z;
}

float3 GetSamplingVector(float2 uv)
{
    // Flip Y and change to -1 -> 1
    uv = 2.0 * float2(uv.x, 1.0 - uv.y) - 1.0;

    float3 ret;
    switch (faceIdx)
    {
        default:
        case 0:
            return normalize(float3(1.0, uv.y, -uv.x));
        case 1:
            return normalize(float3(-1.0, uv.y, uv.x));
        case 2:
            return normalize(float3(uv.x, 1.0, -uv.y));
        case 3:
            return normalize(float3(uv.x, -1.0, uv.y));
        case 4:
            return normalize(float3(uv.x, uv.y, 1.0));
        case 5:
            return normalize(float3(-uv.x, uv.y, -1.0));
    }
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
    uint2 texSize;
    outFace.GetDimensions(texSize.x, texSize.y);

    if (any(threadID.xy > texSize))
    {
        return;
    }

    float2 uv = float2(threadID.xy) / float2(texSize);

    float3 N = GetSamplingVector(uv);
    float3 R = N;
    float3 V = R;

    float3 prefilteredColor = 0.0;
    float totalWeight = 0.0;

    const uint NumSamples = 2048;
    for (uint i = 0; i < NumSamples; i++)
    {
        float2 Xi = Hammersley2D(i, NumSamples);
        float3 H = ImportanceSampleGGX(Xi, N, roughness);
        float3 L = normalize(2.0 * dot(V, H) * H - V);

        float ndl = saturate(dot(N, L));
        if (ndl > 0)
        {
            prefilteredColor += clamp(fullCubemap.SampleLevel(L, 0.0).xyz * ndl, 0.0, 1.0);
            totalWeight += ndl;
        }
    }

    prefilteredColor = prefilteredColor / totalWeight;

    if (outputIsSRGB == 1)
        prefilteredColor = ConvertToSRGB(prefilteredColor);

    outFace[threadID.xy] = float4(prefilteredColor, 1.0f);
}