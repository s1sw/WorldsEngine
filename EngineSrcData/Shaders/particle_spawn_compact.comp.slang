import Particle;

RWStructuredBuffer<Particle> ParticleBuffer;
RWByteAddressBuffer LiveAndSpawnCount;

[vk::push_constant]
cbuffer PC
{
    uint SourceOffset;
    uint DestinationOffset;
    uint MaxCount;
    uint CountOffset;
    float3 EmitterPosition;
    float Time;
}

float random(float2 p)
{
    float2 K1 = float2(
        23.14069263277926, // e^pi (Gelfond's constant)
        2.665144142690225 // 2^sqrt(2) (Gelfond-Schneider constant)
    );
    return frac( cos( dot(p,K1) ) * 12345.6789 );
}

float nrand(float2 uv)
{
    return frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453);
}

uint pcg_hash(uint input)
{
    uint state = input * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

[numthreads(256, 1, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
    Particle particle = ParticleBuffer[threadID.x + SourceOffset];

    bool shouldLive = threadID.x < MaxCount && particle.Lifetime < 10.0 && particle.Lifetime >= 0.0;

    if (shouldLive)
    {
        uint liveCount;
        LiveAndSpawnCount.InterlockedAdd(CountOffset, 1, liveCount);
        ParticleBuffer[liveCount + DestinationOffset] = particle;
    }
    else
    {
        particle.Lifetime = -1.0;
    }

    AllMemoryBarrierWithGroupSync();
    uint liveCount = LiveAndSpawnCount.Load(int(CountOffset));

    if (liveCount + threadID.x < MaxCount && threadID.x < LiveAndSpawnCount.Load(int(CountOffset + 4)))
    {
        Particle newParticle;
        newParticle.Position = EmitterPosition;
        newParticle.Lifetime = 0.01;
        float f = float(pcg_hash(asuint(cos(Time)) ^ threadID.x)) / float(0xffffffffU);
        float f2 = float(pcg_hash(asuint(sin(Time)))) / float(0xffffffffU);

        float2 randVel = (float2(f, f2) * 5.0) - 2.5;
        newParticle.Velocity = float3(randVel.x, -15.0, randVel.y);

        uint outputLoc;
        LiveAndSpawnCount.InterlockedAdd(CountOffset, 1, outputLoc);
        ParticleBuffer[DestinationOffset + outputLoc] = newParticle;
    }
}
