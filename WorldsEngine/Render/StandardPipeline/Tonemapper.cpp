#include <Render/StandardPipeline/Tonemapper.hpp>
#include <Core/AssetDB.hpp>
#include <glm/vec3.hpp>
#include <R2/VKCommandBuffer.hpp>
#include <R2/VKCore.hpp>
#include <R2/VKDescriptorSet.hpp>
#include <R2/VKPipeline.hpp>
#include <R2/VKSampler.hpp>
#include <R2/VKTexture.hpp>
#include <Render/ShaderReflector.hpp>
#include <Render/ShaderCache.hpp>

using namespace R2;

namespace worlds
{
    struct TonemapSettings
    {
        int idx;
        float exposureBias;
        float vignetteRadius;
        float vignetteSoftness;
        glm::vec3 vignetteColor;
        float resolutionScale;
    };

    Tonemapper::Tonemapper(VK::Core* core, VK::Texture* colorBuffer, VK::Texture* target, VK::Texture* bloom)
        : core(core)
        , colorBuffer(colorBuffer)
        , target(target)
        , bloom(bloom)
    {
        AssetID tonemapShader = colorBuffer->GetSamples() == 1 ? AssetDB::pathToId("Shaders/tonemap.comp.spv") : AssetDB::pathToId("Shaders/tonemap_msaa.comp.spv");
        ShaderReflector sr{tonemapShader};

        descriptorSetLayout = sr.createDescriptorSetLayout(core, 0);

        VK::PipelineLayoutBuilder plb{core->GetHandles()};
        plb.DescriptorSet(descriptorSetLayout.Get());
        plb.PushConstants(VK::ShaderStage::Compute, 0, sizeof(TonemapSettings));
        pipelineLayout = plb.Build();

        VK::ComputePipelineBuilder cpb{core};
        cpb .Layout(pipelineLayout.Get())
            .SetShader(ShaderCache::getModule(tonemapShader));

        pipeline = cpb.Build();

        descriptorSet = core->CreateDescriptorSet(descriptorSetLayout.Get());

        VK::SamplerBuilder sb{core};
        sampler = sb.AddressMode(VK::SamplerAddressMode::Repeat)
        .MagFilter(VK::Filter::Linear)
        .MinFilter(VK::Filter::Linear)
        .MipmapMode(VK::SamplerMipmapMode::Linear)
        .Build();

        VK::DescriptorSetUpdater dsu{core->GetHandles(), descriptorSet.Get()};
        sr.bindSampledTexture(dsu, "hdrImage_0", colorBuffer, sampler.Get());
        sr.bindSampledTexture(dsu, "bloomImage_0", bloom, sampler.Get());
        sr.bindStorageTexture(dsu, "resultImage_0", target);

        dsu.Update();
    }

    void Tonemapper::Execute(VK::CommandBuffer& cb)
    {
        bloom->Acquire(cb, VK::ImageLayout::ShaderReadOnlyOptimal, VK::AccessFlags::ShaderRead, VK::PipelineStageFlags::ComputeShader);
        colorBuffer->Acquire(cb, VK::ImageLayout::ShaderReadOnlyOptimal, VK::AccessFlags::ShaderRead, VK::PipelineStageFlags::ComputeShader);
        target->Acquire(cb, VK::ImageLayout::General, VK::AccessFlags::ShaderStorageWrite, VK::PipelineStageFlags::ComputeShader);

        TonemapSettings ts{};
        ts.exposureBias = 0.5f;
        ts.resolutionScale = 1.0f;
        ts.vignetteRadius = 1.0f;

        cb.BindComputeDescriptorSet(pipelineLayout.Get(), descriptorSet->GetNativeHandle(), 0);
        cb.BindComputePipeline(pipeline.Get());
        cb.PushConstants(ts, VK::ShaderStage::Compute, pipelineLayout.Get());
        cb.Dispatch((target->GetWidth() + 15) / 16, (target->GetHeight() + 15) / 16, 1);
    }
}