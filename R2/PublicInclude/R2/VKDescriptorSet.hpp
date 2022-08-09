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
    class Core;
    class Texture;
    class TextureView;
    class Buffer;
    class Sampler;
    enum class ImageLayout : uint32_t;

    class DescriptorSet
    {
    public:
        DescriptorSet(Core* core, VkDescriptorSet set);
        ~DescriptorSet();
        VkDescriptorSet GetNativeHandle();
    private:
        Core* core;
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

    enum class DescriptorType : uint32_t
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
        DescriptorSetLayoutBuilder& PartiallyBound();
        DescriptorSetLayoutBuilder& UpdateAfterBind();
        DescriptorSetLayoutBuilder& VariableDescriptorCount();
        DescriptorSetLayout* Build();
    private:
        struct DescriptorBinding
        {
            uint32_t Binding;
            DescriptorType Type;
            uint32_t Count;
            ShaderStage Stage;
            bool PartiallyBound;
            bool UpdateAfterBind;
            bool VariableDescriptorCount;
        };

        std::vector<DescriptorBinding> bindings;
        const Handles* handles;
    };

    class DescriptorSetUpdater
    {
    public:
        DescriptorSetUpdater(const Handles* handles, DescriptorSet* ds);
        DescriptorSetUpdater& AddTexture(uint32_t binding, uint32_t arrayElement, DescriptorType type, Texture* tex, Sampler* sampler = nullptr);
        DescriptorSetUpdater& AddTextureWithLayout(uint32_t binding, uint32_t arrayElement, DescriptorType type, Texture* tex, ImageLayout layout, Sampler* sampler = nullptr);
        DescriptorSetUpdater& AddTextureView(uint32_t binding, uint32_t arrayElement, DescriptorType type, TextureView* texView, Sampler* sampler = nullptr);
        DescriptorSetUpdater& AddBuffer(uint32_t binding, uint32_t arrayElement, DescriptorType type, Buffer* tex);
        void Update();
    private:
        enum class DSWriteType
        {
            Texture,
            TextureView,
            Buffer
        };

        struct DSWrite
        {
            uint32_t Binding;
            uint32_t ArrayElement;
            DescriptorType Type;
            DSWriteType WriteType;
            ImageLayout TextureLayout;
            
            union
            {
                Texture* Texture;
                TextureView* TextureView;
                Buffer* Buffer;
            };

            Sampler* Sampler;
        };

        std::vector<DSWrite> descriptorWrites;
        const Handles* handles;
        DescriptorSet* ds;
    };
}