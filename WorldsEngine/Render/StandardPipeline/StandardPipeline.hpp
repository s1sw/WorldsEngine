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
    class LightCull;
    class VKTextureManager;
    class CubemapConvoluter;
    class DebugLineDrawer;
    class Frustum;
    class Bloom;

    class StandardPipeline : public IRenderPipeline
    {
        UniquePtr<R2::VK::DescriptorSetLayout> descriptorSetLayout;
        UniquePtr<R2::VK::DescriptorSet> descriptorSets[2];
        UniquePtr<R2::VK::Pipeline> pipeline;
        UniquePtr<R2::VK::Pipeline> depthPrePipeline;
        UniquePtr<R2::VK::PipelineLayout> pipelineLayout;
        UniquePtr<R2::VK::Buffer> multiVPBuffer;
        UniquePtr<R2::VK::Buffer> modelMatrixBuffers[2];
        UniquePtr<R2::VK::Buffer> lightBuffer;
        UniquePtr<R2::VK::Buffer> lightTileBuffer;
        UniquePtr<R2::VK::Texture> depthBuffer;
        UniquePtr<R2::VK::Texture> colorBuffer;

        UniquePtr<Tonemapper> tonemapper;
        UniquePtr<LightCull> lightCull;
        UniquePtr<CubemapConvoluter> cubemapConvoluter;
        UniquePtr<DebugLineDrawer> debugLineDrawer;
        UniquePtr<Bloom> bloom;

        VKRenderer* renderer;
        VKRTTPass* rttPass;

        void drawLoop(entt::registry& reg, R2::VK::CommandBuffer& cb, bool writeMatrices, Frustum& frustum);
        void fillLightBuffer(entt::registry& reg, VKTextureManager* textureManager);
      public:
        StandardPipeline(VKRenderer* renderer);
        ~StandardPipeline();

        void setup(VKRTTPass* rttPass);
        void onResize(int width, int height) override;
        void draw(entt::registry& reg, R2::VK::CommandBuffer& cb) override;
    };
}