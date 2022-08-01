#pragma once
#include <Render/IRenderPipeline.hpp>

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
    class Tonemapper;

    class StandardPipeline : public IRenderPipeline
    {
        R2::VK::DescriptorSetLayout* descriptorSetLayout;
        R2::VK::DescriptorSet* descriptorSet;
        R2::VK::Pipeline* pipeline;
        R2::VK::PipelineLayout* pipelineLayout;
        R2::VK::Buffer* multiVPBuffer;
        R2::VK::Buffer* modelMatrixBuffer;
        R2::VK::Texture* depthBuffer;
        R2::VK::Texture* colorBuffer;

        Tonemapper* tonemapper;

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