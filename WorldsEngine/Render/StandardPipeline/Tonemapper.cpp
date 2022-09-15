#include <Render/StandardPipeline/Tonemapper.hpp>
#include <Core/AssetDB.hpp>
#include <Core/ConVar.hpp>
#include <glm/vec3.hpp>
#include <R2/VK.hpp>
#include <Render/ShaderReflector.hpp>
#include <Render/ShaderCache.hpp>
#include <math.h>

using namespace R2;

namespace worlds
{
    struct TonemapSettings
    {
        int idx;
        float exposure;
        float contrast;
        float saturation;
        float resolutionScale;
        int skipBloom;
    };

    Tonemapper::Tonemapper(VK::Core* core, VK::Texture* colorBuffer, VK::Texture* target, VK::Texture* bloom)
        : core(core)
        , colorBuffer(colorBuffer)
        , target(target)
        , bloom(bloom)
    {
        AssetID tonemapShader = colorBuffer->GetSamples() == 1 ? AssetDB::pathToId("Shaders/tonemap.comp.spv") : AssetDB::pathToId("Shaders/tonemap_msaa.comp.spv");

        if (colorBuffer->GetLayers() > 1)
        {
            tonemapShader = AssetDB::pathToId("Shaders/tonemap_msaa_multivp.comp.spv");
        }

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

        VK::SamplerBuilder sb{core};
        sampler = sb.AddressMode(VK::SamplerAddressMode::Repeat)
        .MagFilter(VK::Filter::Linear)
        .MinFilter(VK::Filter::Linear)
        .MipmapMode(VK::SamplerMipmapMode::Linear)
        .Build();

        for (int i = 0; i < colorBuffer->GetLayers(); i++)
        {
            VK::TextureSubset subset{};
            subset.Dimension = VK::TextureDimension::Dim2D;
            subset.LayerCount = 1;
            subset.LayerStart = i;
            subset.MipCount = 1;
            subset.MipStart = 0;

            outputViews.push_back(new VK::TextureView(core, target, subset));

            descriptorSets.push_back(core->CreateDescriptorSet(descriptorSetLayout.Get()));

            VK::DescriptorSetUpdater dsu{core->GetHandles(), descriptorSets[i].Get()};
            sr.bindSampledTexture(dsu, "hdrImage_0", colorBuffer, sampler.Get());
            sr.bindSampledTexture(dsu, "bloomImage_0", bloom, sampler.Get());
            dsu.AddTextureView(0, 0, VK::DescriptorType::StorageImage, outputViews[i].Get());
            dsu.Update();
        }
    }

    static ConVar exposure{"r_exposure", "3.5", "Sets the rendering exposure."};
    static ConVar contrast{"r_contrast", "0.87", "Sets the rendering contrast."};
    static ConVar saturation{"r_saturation", "1.0", "Sets the rendering saturation."};

    void Tonemapper::Execute(VK::CommandBuffer& cb, bool skipBloom)
    {
        bloom->Acquire(cb, VK::ImageLayout::ShaderReadOnlyOptimal, VK::AccessFlags::ShaderRead, VK::PipelineStageFlags::ComputeShader);
        colorBuffer->Acquire(cb, VK::ImageLayout::ShaderReadOnlyOptimal, VK::AccessFlags::ShaderRead, VK::PipelineStageFlags::ComputeShader);
        target->Acquire(cb, VK::ImageLayout::General, VK::AccessFlags::ShaderStorageWrite, VK::PipelineStageFlags::ComputeShader);

        cb.BindComputePipeline(pipeline.Get());
        for (int i = 0; i < colorBuffer->GetLayers(); i++)
        {
            TonemapSettings ts{};
            ts.exposure = exp2f(exposure.getFloat());
            ts.contrast = contrast.getFloat();
            ts.saturation = saturation.getFloat();
            ts.resolutionScale = 1.0f;
            ts.idx = i;
            ts.skipBloom = (int)skipBloom;

            cb.BindComputeDescriptorSet(pipelineLayout.Get(), descriptorSets[i]->GetNativeHandle(), 0);
            cb.PushConstants(ts, VK::ShaderStage::Compute, pipelineLayout.Get());
            cb.Dispatch((target->GetWidth() + 15) / 16, (target->GetHeight() + 15) / 16, 1);
        }
    }
}