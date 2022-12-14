struct VertexInput
{
    [vk::location(0)]
    float3 Position : POSITION;
    [vk::location(1)]
    float3 Normal : NORMAL;
    [vk::location(2)]
    float3 Tangent : TANGENT;
    [vk::location(3)]
    float BitangentSign : BITANGENTSIGN;
    [vk::location(4)]
    float2 UV : TEXCOORD;
}

struct VertexOutput
{
    float4 Position : SV_Position;
    float3 WorldPosition : WORLDPOS;
    float3 Normal : NORMAL;
    float4 TangentWithSign : TANGENT;
    float2 UV : TEXCOORD;
    nointerpolation uint DrawID : DRAWID;
}

struct MultiVP
{
    float4x4 ViewMatrices[2];
    float4x4 ProjectionMatrices[2];
    float4x4 inverseVP[2];
    float4 viewPos[2];
    int screenWidth;
    int screenHeight;
}

struct PBRMaterial
{
    uint AlbedoTexture;
    uint NormalTexture;
    uint MRATexture;
    float DefaultRoughness;
    float DefaultMetallic;
    float3 EmissiveColor;
    uint CutoffFlags;

    property uint Flags
    {
        get
        {
            return (CutoffFlags & (0x7FFFFF80)) >> 8;
        }
    }

    property float AlphaCutoff
    {
        get
        {
            return (CutoffFlags & 0xFF) / 255.0f;
        }
    }
}

struct DrawInfo
{
    uint MaterialOffset;
    uint ModelMatrixID;
    float2 TextureOffset;
    float2 TextureScale;
}

ConstantBuffer<MultiVP> VPBuffer;
StructuredBuffer<float4x4> ModelMatrices;
ByteAddressBuffer MaterialBuffer;
ByteAddressBuffer RawLightBuffer;
ByteAddressBuffer LightTiles;
StructuredBuffer<DrawInfo> DrawInfos;

cbuffer SceneGlobals 
{
    float time;
    float shadowmapResolution;
    uint blueNoiseTexture;
    float2 PoissonKernel[64];
};

[vk::binding(0, 1)]
[allow("parameterBindingsOverlap")]
Sampler2D<float4> Textures[];

[vk::binding(0, 1)]
[allow("parameterBindingsOverlap")]
SamplerCube<float4> Cubemaps[];

[shader("fragment")]
float4 FragmentShader(VertexOutput input, float2 fragCoord : SV_Position, uint viewIndex : SV_ViewID) : SV_TARGET
{
    float3 viewDir = normalize(VPBuffer.viewPos[viewIndex].xyz - input.WorldPosition.xyz);
    float dp = 1.0 - dot(input.Normal, viewDir);
    float dp2 = dp * dp;
    float3 col = pow(float3(0.0, 0.25, 0.75) * 5, float3(2.2)) * max(dp2, 0.05);
    float2 flowVec = normalize(float2(input.Normal.x, max(input.Normal.y, input.Normal.z)));
    
    return float4(col, dp2 + Textures[NonUniformResourceIndex(blueNoiseTexture)].Sample(fragCoord * float2(1 / 128.0) + flowVec * float2(sin(time * 0.1 + fragCoord.x * 0.0001), cos(time * 0.1 + fragCoord.y * 0.0001))).x * 0.1);
}