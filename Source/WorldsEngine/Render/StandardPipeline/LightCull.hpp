#pragma once
#include <Util/UniquePtr.hpp>
#include <glm/mat4x4.hpp>

namespace R2::VK
{
    class Texture;
    class Buffer;
    class FrameSeparatedBuffer;
    class Pipeline;
    class PipelineLayout;
    class Core;
    class CommandBuffer;
    class DescriptorSet;
    class DescriptorSetLayout;
    class Sampler;
    class TimestampPool;
}

namespace worlds
{
    class SimpleCompute;
    class VKRenderer;

    class LightCull
    {
        VKRenderer* renderer;
        R2::VK::Texture* depthBuffer;
        R2::VK::FrameSeparatedBuffer* lightBuffers;
        R2::VK::Buffer* lightTiles;
        R2::VK::Buffer* multiVPBuffer;
        UniquePtr<R2::VK::Sampler> sampler;
        UniquePtr<SimpleCompute> cs[2];
    public:
        LightCull(VKRenderer* renderer, R2::VK::Texture* depthBuffer, R2::VK::FrameSeparatedBuffer* lightBuffers, R2::VK::Buffer* lightTiles, R2::VK::Buffer* multiVPBuffer);
        void Execute(R2::VK::CommandBuffer& cb);
    };
}