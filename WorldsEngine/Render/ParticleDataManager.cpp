#include "ParticleDataManager.hpp"
#include <Core/Log.hpp>
#include <robin_hood.h>
#include <Render/RenderInternal.hpp>
#include <R2/SubAllocatedBuffer.hpp>
#include <R2/VK.hpp>
#include <Core/WorldComponents.hpp>

using namespace R2;

namespace worlds
{
    const int MAX_PARTICLES = 1'500'000;

    struct ParticleDataManager::ParticleDataStorage
    {
        robin_hood::unordered_node_map<entt::entity, ParticleSystemData> data;
    };

    ParticleDataManager::ParticleDataManager(VKRenderer* renderer)
        : renderer(renderer)
    {
        VK::Core* core = renderer->getCore();
        storage = new ParticleDataStorage();

        VK::BufferCreateInfo particleBufCreateInfo{};
        particleBufCreateInfo.Size = 8 * sizeof(float) * MAX_PARTICLES;
        particleBufCreateInfo.Usage = VK::BufferUsage::Vertex | VK::BufferUsage::Storage;
        particleBuffer = new R2::SubAllocatedBuffer(core, particleBufCreateInfo);

        VK::BufferCreateInfo countBufCreateInfo{};
        countBufCreateInfo.Size = 2 * sizeof(uint32_t) * MAX_PARTICLES;
        countBufCreateInfo.Usage = VK::BufferUsage::Storage;
        countBuffer = new R2::SubAllocatedBuffer(core, countBufCreateInfo);
    }

    ParticleDataManager::~ParticleDataManager()
    {
        VK::Core* core = renderer->getCore();
        for (auto& pair : storage->data)
        {
            particleBuffer->Free(pair.second.bufferA.bufferHandle);
            particleBuffer->Free(pair.second.bufferB.bufferHandle);
        }
    }

    ParticleSystemData& ParticleDataManager::getParticleSystemData(entt::entity entity, ParticleSystem& system)
    {
        // Check if we already have particle data for this entity
        auto dataIterator = storage->data.find(entity);

        if (dataIterator != storage->data.end())
            return dataIterator->second;

       // No particle data, create it
       ParticleSystemData data{};
       data.bufferA = allocateBuffer(system.maxParticles);
       data.bufferB = allocateBuffer(system.maxParticles);
       data.countAndSpawnOffset = countBuffer->Allocate(sizeof(uint32_t) * 2, data.countAndSpawnHandle);

       return storage->data.insert({ entity, data }).first->second;
    }

    void ParticleDataManager::deleteParticleSystemData(entt::entity entity)
    {
        auto dataIterator = storage->data.find(entity);

        if (dataIterator == storage->data.end())
        {
            logWarn("Tried to delete particle system data for entity %u that didn't have any", entity);
            return;
        }

        countBuffer->Free(dataIterator->second.countAndSpawnHandle);
        particleBuffer->Free(dataIterator->second.bufferA.bufferHandle);
        particleBuffer->Free(dataIterator->second.bufferB.bufferHandle);

        storage->data.erase(entity);
    }

    R2::VK::Buffer* ParticleDataManager::getParticleBuffer()
    {
        return particleBuffer->GetBuffer();
    }

    ParticleSystemBuffer ParticleDataManager::allocateBuffer(int maxParticles)
    {
        ParticleSystemBuffer b{};
        b.bufferOffset = particleBuffer->Allocate(8 * sizeof(float) * maxParticles, b.bufferHandle);

        return b;
    }

    R2::VK::Buffer *ParticleDataManager::getLiveCountBuffer()
    {
        return countBuffer->GetBuffer();
    }
}