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
        dslb = new VK::DescriptorSetLayoutBuilder(core);
        pushConstantSize = SIZE_MAX;
    }

    SimpleCompute& SimpleCompute::BindStorageBuffer(uint32_t binding, R2::VK::Buffer* buffer)
    {
        if (dslb)
            dslb->Binding(binding, VK::DescriptorType::StorageBuffer, 1, VK::ShaderStage::Compute);
        descriptorBindings.emplace_back(binding, VK::DescriptorType::StorageBuffer, false, false, buffer);
        return *this;
    }

    SimpleCompute& SimpleCompute::BindUniformBuffer(uint32_t binding, R2::VK::Buffer* buffer)
    {
        if (dslb)
            dslb->Binding(binding, VK::DescriptorType::UniformBuffer, 1, VK::ShaderStage::Compute);
        descriptorBindings.emplace_back(binding, VK::DescriptorType::UniformBuffer, false, false, buffer);
        return *this;
    }

    SimpleCompute& SimpleCompute::BindSampledTexture(uint32_t binding, R2::VK::Texture* texture, R2::VK::Sampler* sampler)
    {
        if (dslb)
            dslb->Binding(binding, VK::DescriptorType::CombinedImageSampler, 1, VK::ShaderStage::Compute);
        descriptorBindings.emplace_back(binding, VK::DescriptorType::CombinedImageSampler, false, false, texture, sampler);
        return *this;
    }

    SimpleCompute& SimpleCompute::BindSampledTextureWithLayout(uint32_t binding, R2::VK::Texture* texture, R2::VK::ImageLayout layout, R2::VK::Sampler* sampler)
    {
        if (dslb)
            dslb->Binding(binding, VK::DescriptorType::CombinedImageSampler, 1, VK::ShaderStage::Compute);
        descriptorBindings.emplace_back(binding, VK::DescriptorType::CombinedImageSampler, false, true, texture, sampler, layout);
        return *this;
    }

    SimpleCompute& SimpleCompute::BindStorageTexture(uint32_t binding, R2::VK::Texture* texture)
    {
        if (dslb)
            dslb->Binding(binding, VK::DescriptorType::StorageImage, 1, VK::ShaderStage::Compute);
        descriptorBindings.emplace_back(binding, VK::DescriptorType::StorageImage, false, false, texture, nullptr);
        return *this;
    }

    SimpleCompute& SimpleCompute::BindSampledTextureView(uint32_t binding, R2::VK::TextureView* textureView, R2::VK::Sampler* sampler)
    {
        if (dslb)
            dslb->Binding(binding, VK::DescriptorType::CombinedImageSampler, 1, VK::ShaderStage::Compute);
        descriptorBindings.emplace_back(binding, VK::DescriptorType::CombinedImageSampler, true, false, textureView, sampler);
        return *this;
    }

    SimpleCompute& SimpleCompute::BindStorageTextureView(uint32_t binding, R2::VK::TextureView* textureView)
    {
        if (dslb)
            dslb->Binding(binding, VK::DescriptorType::StorageImage, 1, VK::ShaderStage::Compute);
        descriptorBindings.emplace_back(binding, VK::DescriptorType::StorageImage, true, false, textureView, nullptr);
        return *this;
    }

    SimpleCompute& SimpleCompute::PushConstantSize(size_t size)
    {
        pushConstantSize = size;
        return *this;
    }

    void SimpleCompute::Build()
    {
        descriptorSetLayout = dslb->Build();
        delete dslb;
        dslb = nullptr;

        descriptorSet = core->CreateDescriptorSet(descriptorSetLayout.Get());

        UpdateDescriptors();

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

    void SimpleCompute::UpdateDescriptors()
    {
        VK::DescriptorSetUpdater dsu{core, descriptorSet.Get()};

        for (const BoundDescriptor& bd : descriptorBindings)
        {
            if (bd.type == VK::DescriptorType::StorageBuffer || bd.type == VK::DescriptorType::UniformBuffer)
            {
                dsu.AddBuffer(bd.binding, 0, bd.type, (VK::Buffer*)bd.pointTo);
            }
            else if (!bd.isTextureView)
            {
                if (bd.useLayout)
                    dsu.AddTextureWithLayout(bd.binding, 0, bd.type, (VK::Texture*)bd.pointTo, bd.layout, bd.sampler);
                else
                    dsu.AddTexture(bd.binding, 0, bd.type, (VK::Texture*)bd.pointTo, bd.sampler);
            }
            else
            {
                dsu.AddTextureView(bd.binding, 0, bd.type, (VK::TextureView*)bd.pointTo, bd.sampler);
            }
        }

        dsu.Update();
        descriptorBindings.clear();
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