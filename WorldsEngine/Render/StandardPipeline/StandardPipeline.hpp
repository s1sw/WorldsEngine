#pragma once
#include <Render/IRenderPipeline.hpp>
#include <Util/UniquePtr.hpp>

namespace R2
{
    class SubAllocatedBuffer;
}

namespace R2::VK
{
    class DescriptorSetLayout;
    class DescriptorSet;
    class Pipeline;
    class PipelineLayout;
    class Buffer;
    class SubAllocatedBuffer;
    class Texture;
    class Core;
}

namespace worlds
{
    class VKRenderer;
    class Tonemapper;

    class StandardPipeline : public IRenderPipeline
    {
        UniquePtr<R2::VK::DescriptorSetLayout> descriptorSetLayout;
        UniquePtr<R2::VK::DescriptorSet> descriptorSet;
        UniquePtr<R2::VK::Pipeline> pipeline;
        UniquePtr<R2::VK::PipelineLayout> pipelineLayout;
        UniquePtr<R2::VK::Buffer> multiVPBuffer;
        UniquePtr<R2::VK::Buffer> modelMatrixBuffer;
        UniquePtr<R2::SubAllocatedBuffer> materialBuffer;
        UniquePtr<R2::VK::Texture> depthBuffer;
        UniquePtr<R2::VK::Texture> colorBuffer;

        UniquePtr<Tonemapper> tonemapper;

        VKRenderer* renderer;
        VKRTTPass* rttPass;
      public:
        StandardPipeline(VKRenderer* renderer);
        ~StandardPipeline();

        void setup(VKRTTPass* rttPass);
        void onResize(int width, int height) override;
        void draw(entt::registry& reg, R2::VK::CommandBuffer& cb) override;
    };
}