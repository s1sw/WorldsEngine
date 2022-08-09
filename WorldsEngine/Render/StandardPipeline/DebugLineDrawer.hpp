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
    struct DebugLine;

    class DebugLineDrawer
    {
        R2::VK::Core* core;
        UniquePtr<R2::VK::Buffer> debugLineBuffer;
        UniquePtr<R2::VK::Pipeline> pipeline;
        UniquePtr<R2::VK::PipelineLayout> pipelineLayout;
        UniquePtr<R2::VK::DescriptorSetLayout> dsl;
        UniquePtr<R2::VK::DescriptorSet> ds;
    public:
        DebugLineDrawer(R2::VK::Core* core, R2::VK::Buffer* vpBuffer, int msaaLevel);
        void Execute(R2::VK::CommandBuffer& cb, const DebugLine* debugLines, size_t debugLineCount);
    };
}