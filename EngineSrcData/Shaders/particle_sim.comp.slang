import Particle;

RWStructuredBuffer<Particle> ParticleBuffer;

[vk::push_constant]
cbuffer PushConstants
{
    uint ParticleOffset;
    uint ParticleCount;
    float DeltaTime;
}

float3 GetClosestPointOnSphere(float3 currPoint, float3 sphereCenter, float sphereRadius)
{
    return sphereCenter + normalize(sphereCenter - currPoint) * sphereRadius;
}

[numthreads(256, 1, 1)]
void main(uint threadID : SV_DispatchThreadID)
{
    if (threadID.x >= ParticleCount) return;
    Particle particle = ParticleBuffer[ParticleOffset + threadID.x];

    if (particle.Lifetime < 0.0) return;

    float3 attractorPos = GetClosestPointOnSphere(particle.Position, float3(0.0, 1.5, 1.5), 5.0);
    float3 dir = attractorPos - particle.Position;
    particle.Velocity += dir * DeltaTime * 20.0;

    //particle.Velocity += float3(0.0, -9.81, 0.0) * DeltaTime;
    particle.Position += particle.Velocity * DeltaTime;
    particle.Lifetime += DeltaTime;

    ParticleBuffer[ParticleOffset + threadID.x] = particle;
}
