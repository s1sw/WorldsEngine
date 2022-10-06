[vk::binding(0)]
RWByteAddressBuffer PickingBuffer;

struct VertexInput
{
    [vk::location(0)]
    float3 Position : POSITION;
}

struct VertexOutput
{
    float4 Position : SV_Position;
}

[vk::push_constant]
cbuffer PC
{
    float4x4 mvp;
    uint pixelX;
    uint pixelY;
    uint entityId;
}

[shader("vertex")]
VertexOutput VertexShader(VertexInput input)
{
    VertexOutput output;
    output.Position = mul(mvp, float4(input.Position, 1.0));
    output.Position.y = -output.Position.y;

    return output;
}

[shader("fragment")]
[earlydepthstencil]
void FragmentShader(VertexOutput input, float2 fragCoord : SV_Position)
{
    int x = int(fragCoord.x);
    int y = int(fragCoord.y);

    if (x == pixelX && y == pixelY)
    {
        PickingBuffer.Store(0, entityId);
    }
}
