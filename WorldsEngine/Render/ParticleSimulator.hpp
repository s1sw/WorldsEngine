#pragma once
#include <entt/entity/lw_fwd.hpp>
#include <Util/UniquePtr.hpp>

namespace R2::VK
{
    class CommandBuffer;
}

namespace worlds
{
    class SimpleCompute;
    class VKRenderer;

    class ParticleSimulator
    {
    public:
        explicit ParticleSimulator(VKRenderer* renderer);
        ~ParticleSimulator();
        void execute(entt::registry& reg, R2::VK::CommandBuffer& cb, float deltaTime);
    private:
        VKRenderer* renderer;
        UniquePtr<SimpleCompute> cs;
        UniquePtr<SimpleCompute> spawnCompactCs;
    };
}