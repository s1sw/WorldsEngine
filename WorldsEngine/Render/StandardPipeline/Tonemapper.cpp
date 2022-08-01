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

    Tonemapper::Tonemapper(VK::Core* core, VK::Texture* colorBuffer, VK::Texture* target)
        : core(core)
        , colorBuffer(colorBuffer)
        , target(target)
    {
        AssetID tonemapShader = AssetDB::pathToId("Shaders/tonemap.comp.spv");
        ShaderReflector sr{tonemapShader};

        descriptorSetLayout = sr.createDescriptorSetLayout(core, 0);

        VK::PipelineLayoutBuilder plb{core->GetHandles()};
        plb.DescriptorSet(descriptorSetLayout);
        plb.PushConstants(VK::ShaderStage::Compute, 0, sizeof(TonemapSettings));
        pipelineLayout = plb.Build();

        VK::ComputePipelineBuilder cpb{core->GetHandles()};
        cpb .Layout(pipelineLayout)
            .SetShader(ShaderCache::getModule(tonemapShader));

        pipeline = cpb.Build();

        descriptorSet = core->CreateDescriptorSet(descriptorSetLayout);

        VK::SamplerBuilder sb{core->GetHandles()};
        sampler = sb.AddressMode(VK::SamplerAddressMode::Repeat)
        .MagFilter(VK::Filter::Linear)
        .MinFilter(VK::Filter::Linear)
        .MipmapMode(VK::SamplerMipmapMode::Linear)
        .Build();

        VK::DescriptorSetUpdater dsu{core->GetHandles(), descriptorSet};
        sr.bindSampledTexture(dsu, "hdrImage_0", colorBuffer, sampler);
        sr.bindStorageTexture(dsu, "resultImage_0", target);

        dsu.Update();
    }

    void Tonemapper::Execute(VK::CommandBuffer& cb)
    {
        colorBuffer->Acquire(cb, VK::ImageLayout::ShaderReadOnlyOptimal, VK::AccessFlags::ShaderSampledRead);
        target->Acquire(cb, VK::ImageLayout::General, VK::AccessFlags::ShaderStorageWrite);

        TonemapSettings ts{};
        ts.exposureBias = 0.5f;
        ts.resolutionScale = 1.0f;
        ts.vignetteRadius = 1.0f;

        cb.BindComputeDescriptorSet(pipelineLayout, descriptorSet->GetNativeHandle(), 0);
        cb.BindComputePipeline(pipeline);
        cb.PushConstants(ts, VK::ShaderStage::Compute, pipelineLayout);
        cb.Dispatch((target->GetWidth() + 15) / 16, (target->GetHeight() + 15) / 16, 1);
    }
}