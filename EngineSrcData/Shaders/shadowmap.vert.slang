struct VertexInput
{
    [[vk::location(0)]]
    float3 Position : POSITION;
}

struct VertexOutput
{
    float4 Position : SV_Position;
}

[[vk::push_constant]]
cbuffer PC
{
    float4x4 mvp;
}

[shader("vertex")]
VertexOutput main(VertexInput input)
{
    VertexOutput output;
    output.Position = mul(mvp, float4(input.Position, 1.0));
    output.Position.y = -output.Position.y;

    return output;
}