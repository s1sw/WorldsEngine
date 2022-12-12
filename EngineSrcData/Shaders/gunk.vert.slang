import Math;
import NormalMapUtils;
import PBRShade;
import PBRUtil;
import Light;
import LightBuffer;
import LightTiles;
import Cubemap;

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

    float3 GetNormal(float2 uv, float3x3 tbn)
    {
        float3 n;

        if (NormalTexture != ~0u)
        {
            n = DecodeNormalMap(Textures[NonUniformResourceIndex(NormalTexture)].Sample(uv).xy);
        }
        else
        {
            n = float3(0.0, 0.0, 1.0);
        }

        return mul(tbn, n);
    }

    void LoadPBRInfo(inout ShadeInfo si, float2 uv)
    {
        if (MRATexture == ~0u)
        {
            si.metallic = DefaultMetallic;
            si.roughness = DefaultRoughness;
            si.ao = 1;
            return;
        }

        float3 texVals = Textures[NonUniformResourceIndex(MRATexture)].Sample(uv).xyz;
        si.metallic = texVals.r;
        si.roughness = texVals.g;
        si.ao = texVals.b;
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


[shader("vertex")]
VertexOutput VertexShader(VertexInput input, uint viewIndex : SV_ViewID, uint drawIndex : SV_InstanceID)
{
    float Amplitude = 1.0;
    float Wavelength = 10.0;
    VertexOutput output;
    DrawInfo di = DrawInfos[drawIndex];

    float4x4 model = ModelMatrices[di.ModelMatrixID];
    float4x4 mvp = mul(VPBuffer.ProjectionMatrices[viewIndex], mul(VPBuffer.ViewMatrices[viewIndex], model));

    float3 vertPos = input.Position;
    float3 origWPos = mul(model, float4(vertPos, 1.0)).xyz;

    vertPos.y += sin(sin(0.1 * time + origWPos.x) * 3.0 * origWPos.z + 2.1 * time) * 0.1 + sin(2.7 * origWPos.x + 2.5 * time) * 0.1 + 0.05;
    float x = origWPos.x;
    float z = origWPos.z;
    float t = time;
    float xT = 0.3 * z * cos(0.1 * t + x) * cos(3 * z * sin(0.1 * t + x) + 2.1 * t) + 0.27 * cos(2.5 * t + 2.7 * x);
    float zT = 0.3 * sin(0.1 * t + x) * cos(3 * z * sin(0.1 * t + x) + 2.1 * t);

    output.Normal = input.Normal;//normalize(float3(xT, 1.0, zT));

    output.Position = mul(mvp, float4(vertPos, 1.0));
    output.WorldPosition = mul(model, float4(vertPos, 1.0)).xyz;
    output.TangentWithSign = float4(input.Tangent, 1.0);
    output.UV = (input.UV * di.TextureScale) + float2(0.0, time * 0.1);
    output.Position.y = -output.Position.y;
    output.DrawID = drawIndex;

    return output;
}