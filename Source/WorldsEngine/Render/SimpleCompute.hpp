#pragma once
#include <stdint.h>
#include <Util/UniquePtr.hpp>
#include <vector>

namespace R2::VK
{
    class Texture;
    class Buffer;
    class Pipeline;
    class PipelineLayout;
    class Core;
    class CommandBuffer;
    class DescriptorSet;
    class DescriptorSetLayout;
    class Sampler;
    class DescriptorSetLayoutBuilder;
    class DescriptorSetUpdater;
    class TextureView;
    enum class DescriptorType : uint32_t;
    enum class ImageLayout : uint32_t;
}

namespace worlds
{
    typedef uint32_t AssetID;

    class SimpleCompute
    {
        struct BoundDescriptor
        {
            uint32_t binding;
            R2::VK::DescriptorType type;
            bool isTextureView;
            bool useLayout;
            void* pointTo;
            R2::VK::Sampler* sampler;
            R2::VK::ImageLayout layout;
        };

        R2::VK::Core* core;
        UniquePtr<R2::VK::PipelineLayout> pipelineLayout;
        UniquePtr<R2::VK::Pipeline> pipeline;
        UniquePtr<R2::VK::DescriptorSetLayout> descriptorSetLayout;
        UniquePtr<R2::VK::DescriptorSet> descriptorSet;
        R2::VK::DescriptorSetLayoutBuilder* dslb;
        AssetID shaderId;
        std::vector<BoundDescriptor> descriptorBindings;
        size_t pushConstantSize;
    public:
        SimpleCompute(R2::VK::Core* core, AssetID shaderId);
        SimpleCompute& BindStorageBuffer(uint32_t binding, R2::VK::Buffer* buffer);
        SimpleCompute& BindUniformBuffer(uint32_t binding, R2::VK::Buffer* buffer);
        SimpleCompute& BindSampledTexture(uint32_t binding, R2::VK::Texture* texture, R2::VK::Sampler* sampler);
        SimpleCompute& BindSampledTextureWithLayout(uint32_t binding, R2::VK::Texture* texture, R2::VK::ImageLayout layout, R2::VK::Sampler* sampler);
        SimpleCompute& BindStorageTexture(uint32_t binding, R2::VK::Texture* texture);
        SimpleCompute& BindSampledTextureView(uint32_t binding, R2::VK::TextureView* texture, R2::VK::Sampler* sampler);
        SimpleCompute& BindStorageTextureView(uint32_t binding, R2::VK::TextureView* texture);
        SimpleCompute& PushConstantSize(size_t size);
        void Build();
        void UpdateDescriptors();
        template <typename T>
        void Dispatch(R2::VK::CommandBuffer& cb, T& pushConstants, uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
        {
            Dispatch(cb, &pushConstants, sizeof(pushConstants), groupsX, groupsY, groupsZ);
        }

        void Dispatch(R2::VK::CommandBuffer& cb, void* pushConstants, size_t pushConstantSize, uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ);
    };
}