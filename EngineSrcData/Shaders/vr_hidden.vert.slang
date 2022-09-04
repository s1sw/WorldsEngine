StructuredBuffer<float2> verts;

[[vk::push_constant]]
cbuffer PC
{
    uint eyeOffset;
};

struct VertexOutput
{
    float4 Position : SV_Position;
};

[shader("vertex")]
VertexOutput main(uint vertexID : SV_VertexID, uint viewIndex : SV_ViewID)
{
    VertexOutput output;

    float2 vPos = verts[vertexID + (eyeOffset * viewIndex)];
    vPos *= 2.0;
    vPos -= 1.0;
    output.Position = float4(vPos, 1.0, 1.0);
    output.Position.y = -output.Position.y;

    return output;
}
