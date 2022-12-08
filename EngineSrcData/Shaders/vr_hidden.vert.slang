struct VertexInput
{
    [vk::location(0)]
    float2 Position : POSITION;
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

[[vk::binding(0)]]
ConstantBuffer<MultiVP> VPBuffer;

[[vk::push_constant]]
cbuffer PC
{
    uint forView;
    uint mode;
};

struct VertexOutput
{
    float4 Position : SV_Position;
};

[shader("vertex")]
VertexOutput main(VertexInput input, uint viewId : SV_ViewID)
{
    VertexOutput output;

    float2 vPos = input.Position;
    if (mode == 0)
        output.Position = mul(VPBuffer.ProjectionMatrices[viewId], float4(vPos, -1.0, 1.0));
    else
        output.Position = float4(vPos, 1.0, 1.0);
    output.Position.y = -output.Position.y;
    output.Position.z = 1.0;

    if (viewId != forView)
    {
        output.Position = 0.0 / 0.0;
    }

    return output;
}
