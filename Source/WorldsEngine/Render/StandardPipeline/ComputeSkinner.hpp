#pragma once
#include <Util/UniquePtr.hpp>
#include <entt/entity/lw_fwd.hpp>

namespace R2::VK
{
    class DescriptorSetLayout;
    class DescriptorSet;
    class Pipeline;
    class PipelineLayout;
    class Buffer;
    class Texture;
    class Core;
    class CommandBuffer;
}

namespace worlds
{
    class SimpleCompute;
    class VKRenderer;

    class ComputeSkinner
    {
        VKRenderer* renderer;
        UniquePtr<R2::VK::Buffer> poseBuffer;
        UniquePtr<SimpleCompute> cs;
    public:
        ComputeSkinner(VKRenderer* renderer);
        void Execute(R2::VK::CommandBuffer& cb, entt::registry& reg);
    };
}