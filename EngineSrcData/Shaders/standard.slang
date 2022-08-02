import Math;
import NormalMapUtils;

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
    float3 Normal : NORMAL;
    float4 TangentWithSign : TANGENT;
    float2 UV : TEXCOORD;
};

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
}

ConstantBuffer<MultiVP> VPBuffer;
StructuredBuffer<float4x4> ModelMatrices;
ByteAddressBuffer MaterialBuffer;

[[vk::binding(0, 1)]]
Sampler2D<float4> Textures[];

[[vk::push_constant]]
cbuffer PushConstants
{
    uint ModelMatrixID;
    uint MaterialOffset;
}

[shader("vertex")]
VertexOutput VertexShader(VertexInput input, uint viewIndex : SV_ViewID)
{
    VertexOutput output;

    float4x4 model = ModelMatrices[ModelMatrixID];
    float4x4 mvp = mul(VPBuffer.ProjectionMatrices[viewIndex], mul(VPBuffer.ViewMatrices[viewIndex], model));
    output.Position = mul(mvp, float4(input.Position, 1.0));
    output.Normal = mul(adjoint(model), input.Normal);
    output.TangentWithSign = float4(mul(adjoint(model), input.Tangent).xyz, input.BitangentSign);
    output.UV = input.UV;
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

    float3 color = Textures[material.AlbedoTexture].Sample(input.UV).xyz;
    color *= clamp(dot(normal, float3(0.0, 1.0, 0.0)), 0.1, 1.0);

    return float4(color, 1.0);
}