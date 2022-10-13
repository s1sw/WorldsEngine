#pragma once
#include <entt/entity/lw_fwd.hpp>
#include <Util/UniquePtr.hpp>

namespace R2::VK
{
    class Buffer;
    class CommandBuffer;
    class Pipeline;
    class PipelineLayout;
    class DescriptorSet;
    class DescriptorSetLayout;
}

namespace worlds
{
    class VKRenderer;

    class ParticleRenderer
    {
    public:
        explicit ParticleRenderer(VKRenderer* renderer, int msaaSamples, uint32_t viewMask, R2::VK::Buffer* vpBuf);
        ~ParticleRenderer();
        void Execute(R2::VK::CommandBuffer& cb, entt::registry& registry);
    private:
        VKRenderer* renderer;
        UniquePtr<R2::VK::PipelineLayout> pipelineLayout;
        UniquePtr<R2::VK::Pipeline> pipeline;
        UniquePtr<R2::VK::DescriptorSetLayout> dsl;
        UniquePtr<R2::VK::DescriptorSet> ds;
    };
}