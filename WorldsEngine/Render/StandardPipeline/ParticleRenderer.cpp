#include "ParticleRenderer.hpp"
#include <entt/entity/registry.hpp>
#include <R2/VK.hpp>
#include <Render/RenderInternal.hpp>
#include <Render/ParticleDataManager.hpp>
#include <Render/ShaderCache.hpp>

using namespace R2;

namespace worlds
{
    struct Particle
    {
        glm::vec3 Position;
        float Lifetime;
        glm::vec3 Velocity;
        float Unused;
    };

    ParticleRenderer::ParticleRenderer(worlds::VKRenderer *renderer, int msaaSamples, uint32_t viewMask, VK::Buffer* vpBuf)
        : renderer(renderer)
    {
        VK::Core* core = renderer->getCore();
        ParticleDataManager* pdm = renderer->getParticleDataManager();

        VK::DescriptorSetLayoutBuilder dslb{core};
        dslb.Binding(0, VK::DescriptorType::StorageBuffer, 1, VK::ShaderStage::AllRaster);
        dslb.Binding(1, VK::DescriptorType::UniformBuffer, 1, VK::ShaderStage::AllRaster);
        dsl = dslb.Build();
        ds = core->CreateDescriptorSet(dsl.Get());
        VK::DescriptorSetUpdater dsu{core, ds.Get()};
        dsu.AddBuffer(0, 0, VK::DescriptorType::StorageBuffer, pdm->getParticleBuffer());
        dsu.AddBuffer(1, 0, VK::DescriptorType::UniformBuffer, vpBuf);
        dsu.Update();

        VK::PipelineLayoutBuilder plb{core};
        plb.DescriptorSet(dsl.Get());
        pipelineLayout = plb.Build();

        VK::ShaderModule& vs = ShaderCache::getModule("Shaders/particle.vert.spv");
        VK::ShaderModule& fs = ShaderCache::getModule("Shaders/particle.frag.spv");

        VK::PipelineBuilder pb{core};
        pb.Layout(pipelineLayout.Get())
          .PrimitiveTopology(VK::Topology::TriangleList)
          .DepthTest(true)
          .DepthCompareOp(VK::CompareOp::Greater)
          .DepthWrite(false)
          .DepthAttachmentFormat(VK::TextureFormat::D32_SFLOAT)
          .ColorAttachmentFormat(VK::TextureFormat::B10G11R11_UFLOAT_PACK32)
          .AddShader(VK::ShaderStage::Vertex, vs)
          .AddShader(VK::ShaderStage::Fragment, fs)
          .CullMode(VK::CullMode::None)
          .MSAASamples(msaaSamples)
          .ViewMask(viewMask)
          .AdditiveBlend(true);

        pipeline = pb.Build();
    }

    ParticleRenderer::~ParticleRenderer() = default;

    void ParticleRenderer::Execute(R2::VK::CommandBuffer &cb, entt::registry &registry)
    {
        ParticleDataManager* pdm = renderer->getParticleDataManager();

        cb.BindPipeline(pipeline.Get());
        cb.BindGraphicsDescriptorSet(pipelineLayout.Get(), ds.Get(), 0);

        auto view = registry.view<ParticleSystem, Transform>();
        view.each(
            [&](entt::entity entity, ParticleSystem& ps, Transform& t)
            {
                ParticleSystemData& data = pdm->getParticleSystemData(entity, ps);
                uint32_t bufferOffset = ps.useBufferB ? data.bufferB.bufferOffset : data.bufferA.bufferOffset;

                cb.Draw(ps.maxParticles * 6, 1, 6 * bufferOffset / sizeof(Particle), 0);
            }
        );
    }
}