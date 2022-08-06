#pragma once
#include <Util/UniquePtr.hpp>
#include <glm/mat4x4.hpp>

namespace R2::VK
{
    class Texture;
    class Buffer;
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
    class LightCull
    {
        R2::VK::Core* core;
        R2::VK::Texture* depthBuffer;
        R2::VK::Buffer* lightBuffer;
        R2::VK::Buffer* lightTiles;
        R2::VK::Buffer* multiVPBuffer;
        UniquePtr<R2::VK::PipelineLayout> pipelineLayout;
        UniquePtr<R2::VK::Pipeline> pipeline;
        UniquePtr<R2::VK::DescriptorSetLayout> descriptorSetLayout;
        UniquePtr<R2::VK::DescriptorSet> descriptorSet;
        UniquePtr<R2::VK::Sampler> sampler;
    public:
        LightCull(R2::VK::Core* core, R2::VK::Texture* depthBuffer, R2::VK::Buffer* lightBuffer, R2::VK::Buffer* lightTiles, R2::VK::Buffer* multiVPBuffer);
        void Execute(R2::VK::CommandBuffer& cb);
    };
}