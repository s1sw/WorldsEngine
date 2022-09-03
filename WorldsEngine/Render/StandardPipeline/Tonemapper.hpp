#pragma once
#include <Util/UniquePtr.hpp>
#include <vector>

namespace R2::VK
{
    class Texture;
    class TextureView;
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
        R2::VK::Texture* bloom;
        UniquePtr<R2::VK::PipelineLayout> pipelineLayout;
        UniquePtr<R2::VK::Pipeline> pipeline;
        UniquePtr<R2::VK::DescriptorSetLayout> descriptorSetLayout;
        std::vector<UniquePtr<R2::VK::DescriptorSet>> descriptorSets;
        std::vector<UniquePtr<R2::VK::TextureView>> outputViews;
        UniquePtr<R2::VK::Sampler> sampler;
    public:
        Tonemapper(R2::VK::Core* core, R2::VK::Texture* colorBuffer, R2::VK::Texture* target, R2::VK::Texture* bloom);
        void Execute(R2::VK::CommandBuffer& cb, bool skipBloom);
    };
}