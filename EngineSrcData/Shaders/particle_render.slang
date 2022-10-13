import Particle;

struct MultiVP
{
    float4x4 ViewMatrices[2];
    float4x4 ProjectionMatrices[2];
    float4x4 inverseVP[2];
    float4 viewPos[2];
    int screenWidth;
    int screenHeight;
}

[vk::binding(0)]
StructuredBuffer<Particle> ParticleBuffer;

[vk::binding(1)]
ConstantBuffer<MultiVP> VPBuffer;

struct VertexOutput
{
    float4 Position : SV_Position;
    float ParticleLifetime;
    float2 UV : TEXCOORD;
}

// 0     o________________o 1 / 3
//       |                |
//       |                |
//       |                |
//       |                |
//       |                |
//       |                |
// 2 / 5 o________________o 4

[shader("vertex")]
VertexOutput VertexShader(uint viewIndex : SV_ViewID, uint vertexID : SV_VertexID)
{
    uint particleIndex = vertexID / 6;
    uint vIdx = vertexID % 6;

    float4x4 vp = mul(VPBuffer.ProjectionMatrices[viewIndex], VPBuffer.ViewMatrices[viewIndex]);

    Particle particle = ParticleBuffer[particleIndex];

    float4 particlePosCs = mul(vp, float4(particle.Position, 1.0));
    float4 vPos = particlePosCs;

    float aspect = float(VPBuffer.screenWidth) / float(VPBuffer.screenHeight);

    float3 BILLBOARD[] = {
          float3(-1, -1, 0), // 0
          float3(1, -1, 0), // 1
          float3(-1, 1, 0), // 2
          float3(-1, 1, 0), // 3
          float3(1, -1, 0), // 4
          float3(1, 1, 0), // 5
    };

    vPos += float4(BILLBOARD[vIdx] * 0.25 * float3(rcp(aspect), 1.0, 0.0), 0.0);

    VertexOutput output;
    output.Position = vPos;
    output.Position.y = -output.Position.y;
    output.UV = (BILLBOARD[vIdx].xy + 1.0) * 0.5;
    if (particle.Lifetime < 0.0)
    {
        output.Position = float4(0.0 / 0.0);
    }
    output.ParticleLifetime = particle.Lifetime;

    return output;
}

[shader("fragment")]
float4 FragmentShader(VertexOutput input, float2 fragCoord : SV_Position) : SV_Target
{
    float2 recenteredUV = input.UV * 2.0 - 1.0;
    float alpha = smoothstep(0.49, 0.51, 1.0 - length(recenteredUV));
    alpha *= 1.0 - smoothstep(7.0, 10.0, input.ParticleLifetime);
    return float4(float3(50.0, 0.0, 0.0) * alpha, 1.0);
}
