#pragma once
#include <stdint.h>
#include <R2/VKEnums.hpp>
#include <vector>

#define VK_DEFINE_HANDLE(object) typedef struct object##_T* object;
VK_DEFINE_HANDLE(VkDescriptorSet)
VK_DEFINE_HANDLE(VkDescriptorSetLayout)
#undef VK_DEFINE_HANDLE


namespace R2::VK
{
    struct Handles;
    class Texture;
    class Buffer;
    class Sampler;

    class DescriptorSet
    {
    public:
        DescriptorSet(const Handles* handles, VkDescriptorSet set);
        ~DescriptorSet();
        VkDescriptorSet GetNativeHandle();
    private:
        const Handles* handles;
        VkDescriptorSet set;
    };

    class DescriptorSetLayout
    {
    public:
        DescriptorSetLayout(const Handles* handles, VkDescriptorSetLayout layout);
        ~DescriptorSetLayout();
        VkDescriptorSetLayout GetNativeHandle();
    private:
        const Handles* handles;
        VkDescriptorSetLayout layout;
    };

    enum class DescriptorType
    {
        Sampler = 0,
        CombinedImageSampler = 1,
        SampledImage = 2,
        StorageImage = 3,
        UniformTexelBuffer = 4,
        StorageTexelBuffer = 5,
        UniformBuffer = 6,
        StorageBuffer = 7,
        UniformBufferDynamic = 8,
        StorageBufferDynamic = 9,
        InputAttachment = 10,
        InlineUniformBlock = 1000138000,
        AccelerationStructure = 1000150000
    };

    class DescriptorSetLayoutBuilder
    {
    public:
        DescriptorSetLayoutBuilder(const Handles* handles);
        DescriptorSetLayoutBuilder& Binding(uint32_t binding, DescriptorType type, uint32_t count, ShaderStage stage);
        DescriptorSetLayout* Build();
    private:
        struct DescriptorBinding
        {
            uint32_t Binding;
            DescriptorType Type;
            uint32_t Count;
            ShaderStage Stage;
        };

        std::vector<DescriptorBinding> bindings;
        const Handles* handles;
    };

    class DescriptorSetUpdater
    {
    public:
        DescriptorSetUpdater(const Handles* handles, DescriptorSet* ds);
        DescriptorSetUpdater& AddTexture(uint32_t binding, uint32_t arrayElement, DescriptorType type, Texture* tex, Sampler* sampler = nullptr);
        DescriptorSetUpdater& AddBuffer(uint32_t binding, uint32_t arrayElement, DescriptorType type, Buffer* tex);
        void Update();
    private:
        struct DSWrite
        {
            uint32_t Binding;
            uint32_t ArrayElement;
            DescriptorType Type;
            Texture* Texture;
            Buffer* Buffer;
            Sampler* Sampler;
        };

        std::vector<DSWrite> descriptorWrites;
        const Handles* handles;
        DescriptorSet* ds;
    };
}