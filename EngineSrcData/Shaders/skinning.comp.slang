import Math;

static const int VERTEX_SIZE = 14 * 4;
struct Vertex
{
    float3 Position;
    float3 Normal;
    float3 Tangent;
    float BitangentSign;
    float2 UV;
    float2 UV2;
}

static const int VERTEX_SKIN_INFO_SIZE = 8 * 4;
struct VertexSkinInfo
{
    int4 Indices;
    float4 Weights;
}

RWByteAddressBuffer VertexBuffer;
StructuredBuffer<VertexSkinInfo> SkinInfoBuffer;
StructuredBuffer<float4x4> Poses;

[vk::push_constant]
cbuffer PushConstants
{
    uint NumVertices;
    uint PoseOffset;
    uint InputOffset;
    uint OutputOffset;
    uint SkinInfoOffset;
}

Vertex LoadVertex(int index)
{
    int offset = VERTEX_SIZE * index;

    Vertex vert;
    vert.Position = VertexBuffer.Load<float3>(offset + 0);
    vert.Normal = VertexBuffer.Load<float3>(offset + 12);
    vert.Tangent = VertexBuffer.Load<float3>(offset + 24);
    vert.BitangentSign = VertexBuffer.Load<float>(offset + 36);
    vert.UV = VertexBuffer.Load<float2>(offset + 40);
    vert.UV2 = VertexBuffer.Load<float2>(offset + 48);

    return vert;
}

void StoreVertex(int index, Vertex vert)
{
    int offset = VERTEX_SIZE * index;

    VertexBuffer.Store<float3>(offset + 0, vert.Position);
    VertexBuffer.Store<float3>(offset + 12, vert.Normal);
    VertexBuffer.Store<float3>(offset + 24, vert.Tangent);
    VertexBuffer.Store<float>(offset + 36, vert.BitangentSign);
    VertexBuffer.Store<float2>(offset + 40, vert.UV);
    VertexBuffer.Store<float2>(offset + 48, vert.UV2);
}

[numthreads(256, 1, 1)]
void main(uint threadID : SV_DispatchThreadID)
{
    // Don't overrun because there will be other meshes packed in
    if (threadID >= NumVertices) return;

    Vertex vert = LoadVertex(int(threadID + InputOffset));
    VertexSkinInfo skinInfo = SkinInfoBuffer[threadID + SkinInfoOffset];

    float weightSum = 0.0;

    // Kinda ugly but gets the job done
    float4x4 transform = float4x4(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

    for (int i = 0; i < 4; i++)
    {
        if (weightSum >= 1.0) break;

        float weight = skinInfo.Weights[i];
        transform += Poses[PoseOffset + skinInfo.Indices[i]] * weight;
        weightSum += weight;
    }

    Vertex transformedVert = vert;
    transformedVert.Position = mul(transform, float4(vert.Position, 1.0)).xyz;
    transformedVert.UV = vert.UV;
    transformedVert.BitangentSign = vert.BitangentSign;

    float3x3 transformAdjoint = adjoint(transform);

    transformedVert.Normal = mul(transformAdjoint, vert.Normal);
    transformedVert.Tangent = mul(transformAdjoint, vert.Tangent);

    StoreVertex(int(threadID + OutputOffset), transformedVert);
}