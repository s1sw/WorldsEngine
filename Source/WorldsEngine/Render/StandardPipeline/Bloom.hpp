#pragma once
#include <Util/UniquePtr.hpp>
#include <vector>

namespace R2::VK
{
    class Core;
    class Texture;
    class TextureView;
    class DescriptorSet;
    class DescriptorSetLayout;
    class Pipeline;
    class PipelineLayout;
    class CommandBuffer;
    class Sampler;
}

namespace worlds
{
    class Bloom
    {
        R2::VK::Core* core;

        R2::VK::Texture* hdrSource;
        UniquePtr<R2::VK::Texture> mipChain;

        UniquePtr<R2::VK::Pipeline> downsample;
        UniquePtr<R2::VK::Pipeline> upsample;
        UniquePtr<R2::VK::PipelineLayout> pipelineLayout;
        
        UniquePtr<R2::VK::Pipeline> seedPipeline;

        UniquePtr<R2::VK::DescriptorSetLayout> dsl;
        UniquePtr<R2::VK::Sampler> sampler;

        std::vector<UniquePtr<R2::VK::DescriptorSet>> mipOutputSets;
        std::vector<UniquePtr<R2::VK::TextureView>> mipOutputViews;
        UniquePtr<R2::VK::DescriptorSet> seedDS;
    public:
        Bloom(R2::VK::Core* core, R2::VK::Texture* hdrSource);
        R2::VK::Texture* GetOutput();
        void Execute(R2::VK::CommandBuffer& cb);
    };
}