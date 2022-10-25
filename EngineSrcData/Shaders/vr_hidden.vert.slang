struct VertexInput
{
    [vk::location(0)]
    float2 Position : POSITION;
}

[[vk::push_constant]]
cbuffer PC
{
    uint forView;
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
    vPos *= 2.0;
    vPos -= 1.0;
    output.Position = float4(vPos, 1.0, 1.0);
    output.Position.y = -output.Position.y;

    if (viewId != forView)
    {
        output.Position = 0.0 / 0.0;
    }

    return output;
}
