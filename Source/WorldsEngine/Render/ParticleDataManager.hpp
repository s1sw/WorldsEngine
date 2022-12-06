#pragma once
#include <Core/Engine.hpp>
#include <entt/entity/lw_fwd.hpp>
#include <Util/UniquePtr.hpp>

namespace R2::VK
{
    class Buffer;
}

#define VK_DEFINE_HANDLE(object) typedef struct object##_T* object;
namespace R2
{
    class SubAllocatedBuffer;
    VK_DEFINE_HANDLE(SubAllocationHandle)
}
#undef VK_DEFINE_HANDLE

namespace worlds
{
    class VKRenderer;
    struct ParticleSystem;

    struct ParticleSystemBuffer
    {
        R2::SubAllocationHandle bufferHandle;
        uint64_t bufferOffset;
    };

    struct ParticleSystemData
    {
        ParticleSystemBuffer bufferA;
        ParticleSystemBuffer bufferB;
        uint32_t countAndSpawnOffset;
        R2::SubAllocationHandle countAndSpawnHandle;
    };

    class ParticleDataManager
    {
    public:
        explicit ParticleDataManager(VKRenderer* renderer);
        ~ParticleDataManager();

        ParticleSystemData& getParticleSystemData(entt::entity entity, ParticleSystem& system);
        void deleteParticleSystemData(entt::entity entity);
        R2::VK::Buffer* getParticleBuffer();
        R2::VK::Buffer* getLiveCountBuffer();
    private:
        struct ParticleDataStorage;
        VKRenderer* renderer;
        UniquePtr<ParticleDataStorage> storage;
        UniquePtr<R2::SubAllocatedBuffer> particleBuffer;
        UniquePtr<R2::SubAllocatedBuffer> countBuffer;

        ParticleSystemBuffer allocateBuffer(int maxParticles);
    };
}