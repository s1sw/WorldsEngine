#pragma once
#include <Render/IRenderPipeline.hpp>
#include <Util/UniquePtr.hpp>
#include <vector>
#include <glm/mat4x4.hpp>

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
    class SkyboxRenderer;

    class StandardPipeline : public IRenderPipeline
    {
        UniquePtr<R2::VK::DescriptorSetLayout> descriptorSetLayout;
        UniquePtr<R2::VK::DescriptorSet> descriptorSets[2];
        UniquePtr<R2::VK::Pipeline> pipeline;
        UniquePtr<R2::VK::Pipeline> depthPrePipeline;
        UniquePtr<R2::VK::PipelineLayout> pipelineLayout;
        UniquePtr<R2::VK::Buffer> multiVPBuffer;
        UniquePtr<R2::VK::Buffer> modelMatrixBuffers[2];
        UniquePtr<R2::VK::Buffer> lightBuffers[2];
        UniquePtr<R2::VK::Buffer> lightTileBuffer;
        UniquePtr<R2::VK::Texture> depthBuffer;
        UniquePtr<R2::VK::Texture> colorBuffer;

        UniquePtr<Tonemapper> tonemapper;
        UniquePtr<LightCull> lightCull;
        UniquePtr<CubemapConvoluter> cubemapConvoluter;
        UniquePtr<DebugLineDrawer> debugLineDrawer;
        UniquePtr<Bloom> bloom;
        UniquePtr<SkyboxRenderer> skyboxRenderer;

        VKRenderer* renderer;
        VKRTTPass* rttPass;

        bool useViewOverrides = false;
        std::vector<glm::mat4> overrideViews;
        std::vector<glm::mat4> overrideProjs;

        void createSizeDependants();
        void drawLoop(entt::registry& reg, R2::VK::CommandBuffer& cb, bool writeMatrices, Frustum* frustums,
                      int numViews);
    public:
        StandardPipeline(VKRenderer* renderer);
        ~StandardPipeline();

        void setup(VKRTTPass* rttPass) override;
        void onResize(int width, int height) override;
        void draw(entt::registry& reg, R2::VK::CommandBuffer& cb) override;
        void setView(int viewIndex, glm::mat4 viewMatrix, glm::mat4 projectionMatrix);
    };
}