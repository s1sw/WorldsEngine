import Particle;

RWStructuredBuffer<Particle> ParticleBuffer;

[vk::push_constant]
cbuffer PushConstants
{
    uint ParticleOffset;
    uint ParticleCount;
    float DeltaTime;
    float Time;
}

float3 GetClosestPointOnSphere(float3 currPoint, float3 sphereCenter, float sphereRadius)
{
    return sphereCenter + normalize(sphereCenter - currPoint) * sphereRadius;
}

float3 CalculateAttractorVel(Particle particle, float3 attractorPos)
{
    //GetClosestPointOnSphere(particle.Position, float3(0.0, 1.5, 1.5), 5.0);
    float3 dir = attractorPos - particle.Position;
    return dir * DeltaTime * 10.0;
}

float3 SphereRepel(Particle particle, float3 sphereCenter, float sphereRadius)
{
    float3 dir = particle.Position - sphereCenter;
    float dist = length(dir);
    float strength = max(sphereRadius - dist, 0.0);
    return dir * strength * strength * DeltaTime * 1000.0;
}

float nrand(float2 uv)
{
    return frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453);
}

[numthreads(256, 1, 1)]
void main(uint threadID : SV_DispatchThreadID)
{
    if (threadID.x >= ParticleCount) return;
    Particle particle = ParticleBuffer[ParticleOffset + threadID.x];

    if (particle.Lifetime < 0.0) return;
    
    //particle.Velocity += CalculateAttractorVel(particle, GetClosestPointOnSphere(particle.Position, float3(0.0, 0.5 * sin(Time * 5.0), 1.5), 5.0));
    //particle.Velocity += CalculateAttractorVel(particle, float3(-5.0 * cos(Time * 2.0), 0.0, 0.0));
    //particle.Velocity += CalculateAttractorVel(particle, float3(0.0, 0.0, 10.0));
    //particle.Velocity += nrand(particle.Position.xz) * 2.0 - 1.0;
    particle.Velocity += SphereRepel(particle, float3(0.0, 0.0, 0.0), 2.0);
    particle.Velocity += CalculateAttractorVel(particle, GetClosestPointOnSphere(particle.Position, float3(0.0, 0.0, 0.0), 2.0));
    

    particle.Velocity += float3(0.0, -9.81, 0.0) * DeltaTime;
    particle.Position += particle.Velocity * DeltaTime;
    particle.Lifetime += DeltaTime;

    ParticleBuffer[ParticleOffset + threadID.x] = particle;
}
