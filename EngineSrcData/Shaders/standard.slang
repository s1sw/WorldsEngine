import Math;
import NormalMapUtils;
import PBRShade;
import PBRUtil;
import Light;

struct VertexInput
{
    [[vk::location(0)]]
    float3 Position : POSITION;
    [[vk::location(1)]]
    float3 Normal : NORMAL;
    [[vk::location(2)]]
    float3 Tangent : TANGENT;
    [[vk::location(3)]]
    float BitangentSign : BITANGENTSIGN;
    [[vk::location(4)]]
    float2 UV : TEXCOORD;
}

struct VertexOutput
{
    float4 Position : SV_Position;
    float3 WorldPosition : WORLDPOS;
    float3 Normal : NORMAL;
    float4 TangentWithSign : TANGENT;
    float2 UV : TEXCOORD;
}

struct MultiVP
{
    float4x4 ViewMatrices[2];
    float4x4 ProjectionMatrices[2];
    float4x4 inverseVP[2];
    float4 viewPos[2];
}

struct PBRMaterial
{
    uint AlbedoTexture;
    uint NormalTexture;
    uint MRATexture;
    float DefaultRoughness;
    float DefaultMetallic;

    float3 GetNormal(float2 uv, float3x3 tbn)
    {
        float3 n;

        if (NormalTexture != ~0u)
        {
            n = DecodeNormalMap(Textures[NormalTexture].Sample(uv).xy);
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
            return;
        }

        float3 texVals = Textures[MRATexture].Sample(uv).xyz;
        si.metallic = texVals.r;
        si.roughness = texVals.g;
    }
}

ConstantBuffer<MultiVP> VPBuffer;
StructuredBuffer<float4x4> ModelMatrices;
ByteAddressBuffer MaterialBuffer;
StructuredBuffer<Light> LightBuffer;

[[vk::binding(0, 1)]]
Sampler2D<float4> Textures[];

[[vk::push_constant]]
cbuffer PushConstants
{
    uint ModelMatrixID;
    uint MaterialOffset;
    uint LightCount;
    uint pad;
    float2 TextureOffset;
}

[shader("vertex")]
VertexOutput VertexShader(VertexInput input, uint viewIndex : SV_ViewID)
{
    VertexOutput output;

    float4x4 model = ModelMatrices[ModelMatrixID];
    float4x4 mvp = mul(VPBuffer.ProjectionMatrices[viewIndex], mul(VPBuffer.ViewMatrices[viewIndex], model));
    output.Position = mul(mvp, float4(input.Position, 1.0));
    output.WorldPosition = mul(model, float4(input.Position, 1.0)).xyz;
    output.Normal = normalize(mul(adjoint(model), input.Normal));
    output.TangentWithSign = float4(mul(adjoint(model), input.Tangent).xyz, input.BitangentSign);
    output.UV = input.UV + TextureOffset;
    output.Position.y = -output.Position.y;

    return output;
}

float3x3 ConstructTBN(VertexOutput input)
{
    float3 tangent = input.TangentWithSign.xyz;
    float3 bitangent = cross(input.Normal, tangent) * input.TangentWithSign.w;

    return transpose(float3x3(tangent, bitangent, input.Normal));
}


[shader("fragment")]
float4 FragmentShader(VertexOutput input) : SV_TARGET
{
    PBRMaterial material = MaterialBuffer.Load<PBRMaterial>(MaterialOffset);

    float3x3 tbn = ConstructTBN(input);
    float3 normal = material.GetNormal(input.UV, tbn);

    ShadeInfo shadeInfo;
    shadeInfo.albedoColor = Textures[material.AlbedoTexture].Sample(input.UV).xyz;
    shadeInfo.normal = normalize(normal);
    shadeInfo.viewDir = normalize(VPBuffer.viewPos[0].xyz - input.WorldPosition.xyz);

    material.LoadPBRInfo(shadeInfo, input.UV);

    float3 lo = 0.0f;
    for (uint i = 0; i < LightCount; i++)
    {
        lo += calculateLighting(LightBuffer[i], shadeInfo, input.WorldPosition.xyz);
    }

    return float4(lo, 1.0);
}