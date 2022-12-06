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
    class Core;
    class Buffer;
    class DescriptorSetLayout;
    class DescriptorSet;
    class FrameSeparatedBuffer;
    class Pipeline;
    class PipelineBuilder;
    class PipelineLayout;
    class Texture;
    struct VertexBinding;
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
    class HiddenMeshRenderer;
    class ComputeSkinner;
    class ParticleRenderer;
    struct EngineInterfaces;

    enum class VariantFlags : uint16_t
    {
        None = 0,
        AlphaTest = 1,
        DepthPrepass = 2
    };

    inline VariantFlags operator|(VariantFlags l, VariantFlags r)
    {
        return (VariantFlags)((uint16_t)l | (uint16_t)r);
    }

    struct StandardDrawCommand
    {
        uint32_t indexCount;
        uint32_t firstIndex;
        uint32_t vertexOffset;
        uint16_t techniqueIdx;
        VariantFlags variantFlags;
    };

    class TechniqueManager
    {
        struct Technique
        {
            uint16_t id;
            R2::VK::Pipeline* pipelineVariants[4];
        };
        std::vector<Technique> techniques;
    public:
        TechniqueManager();
        ~TechniqueManager();
        uint16_t createTechnique();
        // Registers a variant of a technique. Takes ownership of pipeline
        void registerVariant(uint16_t id, R2::VK::Pipeline* pipeline, VariantFlags flags);
        R2::VK::Pipeline* getPipelineVariant(uint16_t techniqueId, VariantFlags flags);
    };

    class StandardPipeline : public IRenderPipeline
    {
        UniquePtr<R2::VK::DescriptorSetLayout> descriptorSetLayout;
        UniquePtr<R2::VK::DescriptorSet> descriptorSets[2];
        UniquePtr<R2::VK::PipelineLayout> pipelineLayout;
        UniquePtr<R2::VK::Buffer> multiVPBuffer;
        UniquePtr<R2::VK::FrameSeparatedBuffer> modelMatrixBuffers;
        UniquePtr<R2::VK::FrameSeparatedBuffer> lightBuffers;
        UniquePtr<R2::VK::Buffer> lightTileBuffer;
        UniquePtr<R2::VK::Buffer> sceneGlobals;
        UniquePtr<R2::VK::FrameSeparatedBuffer> drawInfoBuffers;
        UniquePtr<R2::VK::Texture> depthBuffer;
        UniquePtr<R2::VK::Texture> colorBuffer;

        UniquePtr<Tonemapper> tonemapper;
        UniquePtr<LightCull> lightCull;
        UniquePtr<CubemapConvoluter> cubemapConvoluter;
        UniquePtr<DebugLineDrawer> debugLineDrawer;
        UniquePtr<Bloom> bloom;
        UniquePtr<SkyboxRenderer> skyboxRenderer;
        UniquePtr<HiddenMeshRenderer> hiddenMeshRenderer;
        UniquePtr<ComputeSkinner> computeSkinner;
        UniquePtr<ParticleRenderer> particleRenderer;
        UniquePtr<TechniqueManager> techniqueManager;

        const EngineInterfaces& engineInterfaces;
        VKRTTPass* rttPass;

        bool useViewOverrides = false;
        std::vector<glm::mat4> overrideViews;
        std::vector<glm::mat4> overrideProjs;
        std::vector<StandardDrawCommand> drawCmds;
        uint16_t standardTechnique;

        void createSizeDependants();
        void setupMainPassPipeline(R2::VK::PipelineBuilder& pb, R2::VK::VertexBinding& vb);
        void setupDepthPassPipeline(R2::VK::PipelineBuilder& pb, R2::VK::VertexBinding& vb);
    public:
        StandardPipeline(const EngineInterfaces& engineInterfaces);
        ~StandardPipeline();

        void setup(VKRTTPass* rttPass) override;
        void onResize(int width, int height) override;
        void draw(entt::registry& reg, R2::VK::CommandBuffer& cb) override;
        void setView(int viewIndex, glm::mat4 viewMatrix, glm::mat4 projectionMatrix);
        R2::VK::Texture* getHDRTexture() override;
    };
}