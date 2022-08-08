#include <Render/SimpleCompute.hpp>
#include <R2/VK.hpp>
#include <Render/ShaderCache.hpp>
#include <assert.h>

using namespace R2;

namespace worlds
{
    SimpleCompute::SimpleCompute(VK::Core* core, AssetID shaderId)
        : core(core)
        , shaderId(shaderId)
    {
        dslb = new VK::DescriptorSetLayoutBuilder(core->GetHandles());
        pushConstantSize = SIZE_MAX;
    }

    SimpleCompute& SimpleCompute::BindStorageBuffer(uint32_t binding, R2::VK::Buffer* buffer)
    {
        dslb->Binding(binding, VK::DescriptorType::StorageBuffer, 1, VK::ShaderStage::Compute);
        descriptorBindings.emplace_back(binding, 0, VK::DescriptorType::StorageBuffer, buffer);
    }

    SimpleCompute& SimpleCompute::BindUniformBuffer(uint32_t binding, R2::VK::Buffer* buffer)
    {
        dslb->Binding(binding, VK::DescriptorType::UniformBuffer, 1, VK::ShaderStage::Compute);
        descriptorBindings.emplace_back(binding, 0, VK::DescriptorType::UniformBuffer, buffer);
    }

    SimpleCompute& SimpleCompute::BindSampledTexture(uint32_t binding, R2::VK::Texture* texture, R2::VK::Sampler* sampler)
    {
        dslb->Binding(binding, VK::DescriptorType::CombinedImageSampler, 1, VK::ShaderStage::Compute);
        descriptorBindings.emplace_back(binding, VK::DescriptorType::CombinedImageSampler, texture, sampler);
    }

    SimpleCompute& SimpleCompute::BindStorageTexture(uint32_t binding, R2::VK::Texture* texture)
    {
        dslb->Binding(binding, VK::DescriptorType::StorageImage, 1, VK::ShaderStage::Compute);
        descriptorBindings.emplace_back(binding, VK::DescriptorType::StorageImage, texture, nullptr);
    }

    SimpleCompute& SimpleCompute::PushConstantSize(size_t size)
    {
        pushConstantSize = size;
    }

    void SimpleCompute::Build()
    {
        descriptorSetLayout = dslb->Build();
        delete dslb;

        descriptorSet = core->CreateDescriptorSet(descriptorSetLayout.Get());

        VK::DescriptorSetUpdater dsu{core->GetHandles(), descriptorSet.Get()};

        for (const BoundDescriptor& bd : descriptorBindings)
        {
            if (bd.type == VK::DescriptorType::StorageBuffer || bd.type == VK::DescriptorType::UniformBuffer)
            {
                dsu.AddBuffer(bd.binding, 0, bd.type, (VK::Buffer*)bd.pointTo);
            }
            else
            {
                dsu.AddTexture(bd.binding, 0, bd.type, (VK::Texture*)bd.pointTo, bd.sampler);
            }
        }

        dsu.Update();

        VK::PipelineLayoutBuilder plb{core->GetHandles()};
        plb.DescriptorSet(descriptorSetLayout.Get());

        if (pushConstantSize != SIZE_MAX)
        {
            plb.PushConstants(VK::ShaderStage::Compute, 0, pushConstantSize);
        }

        pipelineLayout = plb.Build();

        VK::ComputePipelineBuilder cpb{core};
        cpb.SetShader(ShaderCache::getModule(shaderId));
        cpb.Layout(pipelineLayout.Get());
        pipeline = cpb.Build();
    }

    void SimpleCompute::Dispatch(R2::VK::CommandBuffer& cb, void* pushConstants, size_t pushConstantSize, uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
    {
        assert(pushConstantSize == this->pushConstantSize);

        cb.BindComputePipeline(pipeline.Get());
        cb.BindComputeDescriptorSet(pipelineLayout.Get(), descriptorSet->GetNativeHandle(), 0);
        if (pushConstantSize != SIZE_MAX && pushConstants != nullptr)
            cb.PushConstants(pushConstants, pushConstantSize, VK::ShaderStage::Compute, pipelineLayout.Get());
        cb.Dispatch(groupsX, groupsY, groupsZ);
    }
}