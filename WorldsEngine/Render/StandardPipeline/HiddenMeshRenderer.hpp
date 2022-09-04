#pragma once
#include <stdint.h>
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
    class HiddenMeshRenderer
    {
        R2::VK::Core* core;
        UniquePtr<R2::VK::Buffer> vertBuffer;
        UniquePtr<R2::VK::Pipeline> pipeline;
        UniquePtr<R2::VK::PipelineLayout> pipelineLayout;
        UniquePtr<R2::VK::DescriptorSetLayout> dsl;
        UniquePtr<R2::VK::DescriptorSet> ds;
        uint32_t totalVertexCount;
        uint32_t viewOffset;
    public:
        HiddenMeshRenderer(R2::VK::Core* core, int sampleCount);
        void Execute(R2::VK::CommandBuffer& cb);
    };
}