#include <Render/StandardPipeline/SkyboxRenderer.hpp>
#include <Core/AssetDB.hpp>
#include <R2/VK.hpp>
#include <Render/ShaderCache.hpp>

using namespace R2;

namespace worlds
{
    SkyboxRenderer::SkyboxRenderer(R2::VK::Core* core, R2::VK::PipelineLayout* pipelineLayout, int msaaLevel, unsigned int viewMask)
        : core(core)
        , pipelineLayout(pipelineLayout)
    {
        VK::ShaderModule& vs = ShaderCache::getModule(AssetDB::pathToId("Shaders/skybox.vert.spv"));
        VK::ShaderModule& fs = ShaderCache::getModule(AssetDB::pathToId("Shaders/skybox.frag.spv"));
        VK::PipelineBuilder pb{core};
        pb.Layout(pipelineLayout);
        pb.PrimitiveTopology(VK::Topology::TriangleList)
            .DepthTest(true)
            .DepthCompareOp(VK::CompareOp::GreaterOrEqual)
            .DepthWrite(true)
            .DepthAttachmentFormat(VK::TextureFormat::D32_SFLOAT)
            .ColorAttachmentFormat(VK::TextureFormat::B10G11R11_UFLOAT_PACK32)
            .AddShader(VK::ShaderStage::Vertex, vs)
            .AddShader(VK::ShaderStage::Fragment, fs)
            .CullMode(VK::CullMode::Front)
            .MSAASamples(msaaLevel)
            .ViewMask(viewMask);
        
        pipeline = pb.Build();
    }

    void SkyboxRenderer::Execute(R2::VK::CommandBuffer& cb)
    {
        cb.BeginDebugLabel("Skybox", 0.1f, 0.6f, 1.0f);
        cb.BindPipeline(pipeline.Get());
        cb.Draw(36, 1, 0, 0);
        cb.EndDebugLabel();
    }
}