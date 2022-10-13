#include "ParticleSimulator.hpp"
#include <Core/AssetDB.hpp>
#include <Render/RenderInternal.hpp>
#include <Render/SimpleCompute.hpp>
#include <Render/ParticleDataManager.hpp>
#include <R2/VK.hpp>

using namespace R2;

namespace worlds
{
    struct ParticleSimPushConstants
    {
        uint32_t particleOffset;
        uint32_t particleCount;
        float deltaTime;
    };

    struct ParticleSpawnCompactPushConstants
    {
        uint32_t SourceOffset;
        uint32_t DestinationOffset;
        uint32_t MaxCount;
        uint32_t CountOffset;
        glm::vec3 EmitterPosition;
        float Time;
    };

    struct Particle
    {
        glm::vec3 Position;
        float Lifetime;
        glm::vec3 Velocity;
        float Unused;
    };

    ParticleSimulator::ParticleSimulator(VKRenderer *renderer)
        : renderer(renderer)
    {
        cs = new SimpleCompute(renderer->getCore(), AssetDB::pathToId("Shaders/particle_sim.comp.spv"));
        cs->PushConstantSize(sizeof(ParticleSimPushConstants));
        cs->BindStorageBuffer(0, renderer->getParticleDataManager()->getParticleBuffer());
        cs->Build();

        spawnCompactCs = new SimpleCompute(renderer->getCore(), AssetDB::pathToId("Shaders/particle_spawn_compact.comp.spv"));
        spawnCompactCs->PushConstantSize(sizeof(ParticleSpawnCompactPushConstants));
        spawnCompactCs->BindStorageBuffer(0, renderer->getParticleDataManager()->getParticleBuffer());
        spawnCompactCs->BindStorageBuffer(1, renderer->getParticleDataManager()->getLiveCountBuffer());
        spawnCompactCs->Build();
    }

    ParticleSimulator::~ParticleSimulator() = default;

    void ParticleSimulator::execute(entt::registry &reg, R2::VK::CommandBuffer &cb, float deltaTime)
    {
        cb.BeginDebugLabel("Particle Simulation", 0.702f, 0.380f, 0.168f);
        ParticleDataManager* particleDataManager = renderer->getParticleDataManager();
        VK::Buffer* particleBuffer = particleDataManager->getParticleBuffer();
        VK::Buffer* liveCountBuffer = particleDataManager->getLiveCountBuffer();

        reg.view<ParticleSystem, Transform>().each(
            [&](entt::entity entity, ParticleSystem& ps, Transform& t)
            {
                ps.useBufferB = !ps.useBufferB;
                if (ps.settingsDirty)
                {
                    ps.initialized = false;
                    ps.settingsDirty = false;
                    particleDataManager->deleteParticleSystemData(entity);
                }

                ParticleSystemData& data = particleDataManager->getParticleSystemData(entity, ps);

                uint32_t sourceOffset;
                uint32_t destinationOffset;

                if (ps.useBufferB)
                {
                    sourceOffset = (uint32_t)data.bufferB.bufferOffset;
                    destinationOffset = (uint32_t)data.bufferA.bufferOffset;
                }
                else
                {
                    sourceOffset = (uint32_t)data.bufferA.bufferOffset;
                    destinationOffset = (uint32_t)data.bufferB.bufferOffset;
                }

                liveCountBuffer->Acquire(cb, VK::AccessFlags::TransferWrite, VK::PipelineStageFlags::Transfer);
                if (!ps.initialized)
                {
                    float minusOne = -1.0f;
                    uint32_t minusOneUint;
                    memcpy(&minusOneUint, &minusOne, sizeof(uint32_t));
                    cb.FillBuffer(particleBuffer, sourceOffset, sizeof(Particle) * ps.maxParticles, minusOneUint);
                    cb.FillBuffer(particleBuffer, destinationOffset, sizeof(Particle) * ps.maxParticles, minusOneUint);
                    ps.initialized = true;
                }

                uint32_t zero = 0;
                cb.UpdateBuffer(liveCountBuffer, data.countAndSpawnOffset, sizeof(uint32_t), &zero);
                uint32_t spawnCount = ps.emissionRate;
                cb.UpdateBuffer(liveCountBuffer, data.countAndSpawnOffset + sizeof(uint32_t), sizeof(uint32_t), &spawnCount);

                ParticleSpawnCompactPushConstants spawnCompactPushConstants{};
                spawnCompactPushConstants.SourceOffset = sourceOffset / sizeof(Particle);
                spawnCompactPushConstants.DestinationOffset = destinationOffset / sizeof(Particle);
                spawnCompactPushConstants.MaxCount = ps.maxParticles;
                spawnCompactPushConstants.CountOffset = data.countAndSpawnOffset / sizeof(uint32_t);
                spawnCompactPushConstants.EmitterPosition = t.position;
                spawnCompactPushConstants.Time = (float)renderer->getTime();

                particleBuffer->Acquire(cb, VK::AccessFlags::ShaderReadWrite, VK::PipelineStageFlags::ComputeShader);
                liveCountBuffer->Acquire(cb, VK::AccessFlags::ShaderReadWrite, VK::PipelineStageFlags::ComputeShader);

                uint32_t numGroups = (ps.maxParticles + 255) / 256;
                spawnCompactCs->Dispatch(cb, spawnCompactPushConstants, numGroups, 1, 1);

                ParticleSimPushConstants pspc{};
                pspc.particleCount = ps.maxParticles;
                pspc.particleOffset = destinationOffset / sizeof(Particle);
                pspc.deltaTime = deltaTime;

                particleBuffer->Acquire(cb, VK::AccessFlags::ShaderRead, VK::PipelineStageFlags::ComputeShader);
                cs->Dispatch(cb, pspc, numGroups, 1, 1);
            }
        );

        cb.EndDebugLabel();

        particleBuffer->Acquire(cb, VK::AccessFlags::ShaderRead,
                                VK::PipelineStageFlags::VertexShader | VK::PipelineStageFlags::FragmentShader);
    }
}
