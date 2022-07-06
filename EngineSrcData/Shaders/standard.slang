struct VertexInput
{
    [[vk::location(0)]]
    float3 Position : POSITION;
    [[vk::location(1)]]
    float3 Normal : NORMAL;
}

struct VertexOutput
{
    float4 Position : SV_Position;
    float3 Normal : NORMAL;
};

struct MultiVP
{
    float4x4 ViewMatrices[2];
    float4x4 ProjectionMatrices[2];
    float4x4 inverseVP[2];
    float4 viewPos[2];
}

ConstantBuffer<MultiVP> VPBuffer;
StructuredBuffer<float4x4> ModelMatrices;

[[vk::push_constant]]
cbuffer PushConstants
{
    uint ModelMatrixID;
}

[shader("vertex")]
VertexOutput VertexShader(VertexInput input, uint viewIndex : SV_ViewID)
{
    VertexOutput output;

    float4x4 model = ModelMatrices[ModelMatrixID];
    float4x4 mvp = mul(VPBuffer.ProjectionMatrices[viewIndex], mul(VPBuffer.ViewMatrices[viewIndex], model));
    output.Position = mul(mvp, float4(input.Position, 1.0));
    output.Normal = mul(model, float4(input.Normal, 0.0)).xyz;
    output.Position.y = -output.Position.y;

    return output;
}

[shader("fragment")]
float4 FragmentShader(VertexOutput input) : SV_TARGET
{
    float3 color = float3(0.713, 0.769, 0.859);
    color *= clamp(dot(input.Normal, float3(0.0, 1.0, 0.0)), 0.1, 1.0);
    return float4(color, 1.0);
}