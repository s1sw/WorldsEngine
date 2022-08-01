#pragma once
#include <Util/UniquePtr.hpp>

namespace R2::VK
{
    class Texture;
    class Pipeline;
    class PipelineLayout;
    class Core;
    class CommandBuffer;
    class DescriptorSet;
    class DescriptorSetLayout;
    class Sampler;
}

namespace worlds
{
    class Tonemapper
    {
        R2::VK::Core* core;
        R2::VK::Texture* colorBuffer;
        R2::VK::Texture* target;
        UniquePtr<R2::VK::PipelineLayout> pipelineLayout;
        UniquePtr<R2::VK::Pipeline> pipeline;
        UniquePtr<R2::VK::DescriptorSetLayout> descriptorSetLayout;
        UniquePtr<R2::VK::DescriptorSet> descriptorSet;
        UniquePtr<R2::VK::Sampler> sampler;
    public:
        Tonemapper(R2::VK::Core* core, R2::VK::Texture* colorBuffer, R2::VK::Texture* target);
        void Execute(R2::VK::CommandBuffer& cb);
    };
}