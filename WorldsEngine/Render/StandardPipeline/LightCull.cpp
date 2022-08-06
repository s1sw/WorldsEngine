#include <Render/StandardPipeline/LightCull.hpp>
#include <Core/AssetDB.hpp>
#include <glm/mat4x4.hpp>
#include <R2/VK.hpp>
#include <Render/ShaderCache.hpp>

using namespace R2;

namespace worlds
{
    struct LightCullPushConstants
    {
        uint32_t eyeIndex;
    };

    LightCull::LightCull(VK::Core* core, VK::Texture* depthBuffer, VK::Buffer* lightBuffer, VK::Buffer* lightTiles, VK::Buffer* multiVPBuffer)
        : core(core)
        , depthBuffer(depthBuffer)
        , lightBuffer(lightBuffer)
        , lightTiles(lightTiles)
        , multiVPBuffer(multiVPBuffer)
    {
        VK::SamplerBuilder sb{core->GetHandles()};
        sampler = sb.Build();

        std::string shaderPath = "Shaders/light_cull";

        if (depthBuffer->GetSamples() != 1)
        {
            // MSAA
            shaderPath += "_msaa.comp.spv";
        }
        else
        {
            shaderPath += ".comp.spv";
        }

        AssetID lightCullShader = AssetDB::pathToId(shaderPath);

        VK::DescriptorSetLayoutBuilder dslb{core->GetHandles()};
        dslb.Binding(0, VK::DescriptorType::UniformBuffer, 1, VK::ShaderStage::Compute);
        dslb.Binding(1, VK::DescriptorType::CombinedImageSampler, 1, VK::ShaderStage::Compute);
        dslb.Binding(2, VK::DescriptorType::StorageBuffer, 1, VK::ShaderStage::Compute);
        dslb.Binding(3, VK::DescriptorType::StorageBuffer, 1, VK::ShaderStage::Compute);

        descriptorSetLayout = dslb.Build();
        descriptorSet = core->CreateDescriptorSet(descriptorSetLayout.Get());

        VK::DescriptorSetUpdater dsu{core->GetHandles(), descriptorSet.Get()};
        dsu.AddBuffer(0, 0, VK::DescriptorType::UniformBuffer, multiVPBuffer);
        dsu.AddTexture(1, 0, VK::DescriptorType::CombinedImageSampler, depthBuffer, sampler.Get());
        dsu.AddBuffer(2, 0, VK::DescriptorType::StorageBuffer, lightBuffer);
        dsu.AddBuffer(3, 0, VK::DescriptorType::StorageBuffer, lightTiles);

        dsu.Update();

        VK::PipelineLayoutBuilder plb{core->GetHandles()};
        plb.DescriptorSet(descriptorSetLayout.Get());
        plb.PushConstants(VK::ShaderStage::Compute, 0, sizeof(LightCullPushConstants));
        pipelineLayout = plb.Build();

        VK::ComputePipelineBuilder cpb{core};
        cpb.Layout(pipelineLayout.Get());
        cpb.SetShader(ShaderCache::getModule(lightCullShader));
        pipeline = cpb.Build();
    }

    void LightCull::Execute(VK::CommandBuffer& cb)
    {
        depthBuffer->Acquire(cb, VK::ImageLayout::ShaderReadOnlyOptimal, VK::AccessFlags::ShaderSampledRead);
        lightTiles->Acquire(cb, VK::AccessFlags::ShaderWrite, VK::PipelineStageFlags::ComputeShader);
        cb.BindComputePipeline(pipeline.Get());
        cb.BindComputeDescriptorSet(pipelineLayout.Get(), descriptorSet->GetNativeHandle(), 0);
        LightCullPushConstants pcs{};
        pcs.eyeIndex = 0;

        int w = depthBuffer->GetWidth();
        int h = depthBuffer->GetHeight();
        cb.Dispatch((w + 15) / 16, (h + 15) / 16, 1);
    }
}