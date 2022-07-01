struct VertexInput
{
    [[vk::location(0)]]
    float3 Position : POSITION;
}

struct VertexOutput
{
    float4 Position : SV_Position;
};

struct MultiVP
{
    float4x4 ViewMatrices[2];
    float4x4 ProjectionMatrices[2];
}

ConstantBuffer<MultiVP> VPBuffer;
StructuredBuffer<float4x4> ModelMatrices;

cbuffer PushConstants
{
    uint ModelMatrixID;
}

[shader("vertex")]
VertexOutput VertexShader(VertexInput input, uint viewIndex : SV_ViewID)
{
    VertexOutput output;

    float4x4 mvp = mul(VPBuffer.ProjectionMatrices[viewIndex], mul(VPBuffer.ViewMatrices[viewIndex], ModelMatrices[ModelMatrixID]));
    output.Position = mul(mvp, float4(input.Position, 1.0));
    output.Position.y = -output.Position.y;

    return output;
}

[shader("fragment")]
float4 FragmentShader(VertexOutput input) : SV_TARGET
{
    return float4(1.0);
}