#pragma once
#include <Util/UniquePtr.hpp>

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
    class SkyboxRenderer
    {
        R2::VK::Core* core;
        R2::VK::PipelineLayout* pipelineLayout;
        UniquePtr<R2::VK::Pipeline> pipeline;
    public:
        SkyboxRenderer(R2::VK::Core* core, R2::VK::PipelineLayout* pipelineLayout, int msaaLevel, unsigned int viewMask);
        void Execute(R2::VK::CommandBuffer& cb);
    };
}