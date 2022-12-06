#pragma once
#include <Render/IRenderPipeline.hpp>

#define VK_DEFINE_HANDLE(object) typedef struct object##_T* object;
VK_DEFINE_HANDLE(VkPipelineLayout)
#undef VK_DEFINE_HANDLE

namespace R2::VK
{
    class DescriptorSetLayout;
    class DescriptorSet;
    class Pipeline;
    class PipelineLayout;
    class Buffer;
    class Texture;
    class Core;
}

namespace worlds
{
    class VKRenderer;

    class FakeLitPipeline : public IRenderPipeline
    {
        R2::VK::DescriptorSetLayout* descriptorSetLayout;
        R2::VK::DescriptorSet* descriptorSet;
        R2::VK::Pipeline* pipeline;
        R2::VK::PipelineLayout* pipelineLayout;
        R2::VK::Buffer* multiVPBuffer;
        R2::VK::Buffer* modelMatrixBuffer;
        R2::VK::Texture* depthBuffer;

        VKRenderer* renderer;
        VKRTTPass* rttPass;

      public:
        FakeLitPipeline(VKRenderer* renderer);
        ~FakeLitPipeline();

        void setup(VKRTTPass* rttPass) override;
        void onResize(int width, int height) override;
        void draw(entt::registry& reg, R2::VK::CommandBuffer& cb) override;
    };
}