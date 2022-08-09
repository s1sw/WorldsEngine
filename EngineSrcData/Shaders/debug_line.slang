struct VertexInput
{
    [[vk::location(0)]]
    float3 Position : POSITION;
    [[vk::location(1)]]
    float4 Color : COLOR;
}

struct VertexOutput
{
    float4 Position : SV_Position;
    float4 Color : COLOR;
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

ConstantBuffer<MultiVP> VPBuffer;

[shader("vertex")]
VertexOutput VertexShader(VertexInput input, uint viewIndex : SV_ViewID)
{
    VertexOutput output;

    float4x4 vp = mul(VPBuffer.ProjectionMatrices[viewIndex], VPBuffer.ViewMatrices[viewIndex]);
    output.Position = mul(vp, float4(input.Position, 1.0));
    output.Color = input.Color;
    output.Position.y = -output.Position.y;

    return output;
}

[shader("fragment")]
float4 FragmentShader(VertexOutput input) : SV_Target
{
    return input.Color;
}