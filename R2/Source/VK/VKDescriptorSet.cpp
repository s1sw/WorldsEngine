#include <R2/VKDescriptorSet.hpp>
#include <R2/VKTexture.hpp>
#include <R2/VKBuffer.hpp>
#include <R2/VKCore.hpp>
#include <R2/VKSampler.hpp>
#include <R2/VKDeletionQueue.hpp>
#include <volk.h>

namespace R2::VK
{
    int allocatedDescriptorSets = 0;

    DescriptorSet::DescriptorSet(Core* core, VkDescriptorSet set)
        : core(core)
        , set(set)
    {
        allocatedDescriptorSets++;
    }

    VkDescriptorSet DescriptorSet::GetNativeHandle()
    {
        return set;
    }

    DescriptorSet::~DescriptorSet()
    {
        DeletionQueue* dq = core->perFrameResources[core->frameIndex].DeletionQueue;
        dq->QueueDescriptorSetFree(core->GetHandles()->DescriptorPool, set);
        allocatedDescriptorSets--;
    }

    DescriptorSetLayout::DescriptorSetLayout(Core* core, VkDescriptorSetLayout layout)
        : core(core)
        , layout(layout)
    {}

    VkDescriptorSetLayout DescriptorSetLayout::GetNativeHandle()
    {
        return layout;
    }

    DescriptorSetLayout::~DescriptorSetLayout()
    {
        const Handles* handles = core->GetHandles();
        vkDestroyDescriptorSetLayout(handles->Device, layout, handles->AllocCallbacks);
    }

    DescriptorSetLayoutBuilder::DescriptorSetLayoutBuilder(Core* core)
        : core(core)
    {}

    DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::Binding(uint32_t binding, DescriptorType type, uint32_t count, ShaderStage stage)
    {
        DescriptorBinding db{};
        db.Binding = binding;
        db.Type = type;
        db.Count = count;
        db.Stage = stage;

        bindings.push_back(db);
        
        return *this;
    }

    DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::PartiallyBound()
    {
        bindings.back().PartiallyBound = true;
        return *this;
    }

    DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::UpdateAfterBind()
    {
        bindings.back().UpdateAfterBind = true;
        return *this;
    }

    DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::VariableDescriptorCount()
    {
        bindings.back().VariableDescriptorCount = true;
        return *this;
    }

    DescriptorSetLayout* DescriptorSetLayoutBuilder::Build()
    {
        std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
        std::vector<VkDescriptorBindingFlags> bindingFlags;
        layoutBindings.reserve(bindings.size());
        bindingFlags.reserve(bindings.size());

        bool hasUpdateAfterBind = false;

        for (DescriptorBinding& db : bindings)
        {
            VkDescriptorSetLayoutBinding lb{};
            lb.binding = db.Binding;
            lb.descriptorCount = db.Count;
            lb.descriptorType = static_cast<VkDescriptorType>(db.Type);
            lb.stageFlags = static_cast<VkShaderStageFlags>(db.Stage);

            layoutBindings.push_back(lb);

            VkDescriptorBindingFlags thisBindFlags = 0;

            if (db.PartiallyBound)
            {
                thisBindFlags |= VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
            }

            if (db.UpdateAfterBind)
            {
                thisBindFlags |= VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
                hasUpdateAfterBind = true;
            }

            if (db.VariableDescriptorCount)
            {
                thisBindFlags |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
            }

            bindingFlags.push_back(thisBindFlags);
        }

        VkDescriptorSetLayoutCreateInfo dslci{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        dslci.bindingCount = (uint32_t)layoutBindings.size();
        dslci.pBindings = layoutBindings.data();
        if (hasUpdateAfterBind)
        {
            dslci.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        }

        VkDescriptorSetLayoutBindingFlagsCreateInfo bindFlagsCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO };
        bindFlagsCreateInfo.pBindingFlags = bindingFlags.data();
        bindFlagsCreateInfo.bindingCount = bindingFlags.size();

        dslci.pNext = &bindFlagsCreateInfo;

        const Handles* handles = core->GetHandles();

        VkDescriptorSetLayout dsl;
        VKCHECK(vkCreateDescriptorSetLayout(handles->Device, &dslci, handles->AllocCallbacks, &dsl));

        return new DescriptorSetLayout(core, dsl);
    }

    DescriptorSetUpdater::DescriptorSetUpdater(Core* core, DescriptorSet* ds)
        : handles(core->GetHandles())
        , ds(ds)
    {}

    DescriptorSetUpdater& DescriptorSetUpdater::AddTexture(uint32_t binding, uint32_t arrayElement, DescriptorType type, Texture* tex, Sampler* samp)
    {
        DSWrite dw{};
        dw.Binding = binding;
        dw.ArrayElement = arrayElement;
        dw.Type = type;
        dw.WriteType = DSWriteType::Texture;
        dw.TextureLayout = ImageLayout::Undefined;
        dw.Texture = tex;
        dw.Sampler = samp;

        descriptorWrites.push_back(dw);

        return *this;
    }

    DescriptorSetUpdater& DescriptorSetUpdater::AddTextureWithLayout(uint32_t binding, uint32_t arrayElement, DescriptorType type, Texture* tex, ImageLayout layout, Sampler* samp)
    {
        DSWrite dw{};
        dw.Binding = binding;
        dw.ArrayElement = arrayElement;
        dw.Type = type;
        dw.WriteType = DSWriteType::Texture;
        dw.TextureLayout = layout;
        dw.Texture = tex;
        dw.Sampler = samp;

        descriptorWrites.push_back(dw);

        return *this;
    }

    DescriptorSetUpdater& DescriptorSetUpdater::AddTextureView(uint32_t binding, uint32_t arrayElement, DescriptorType type, TextureView* texView, Sampler* samp)
    {
        DSWrite dw{};
        dw.Binding = binding;
        dw.ArrayElement = arrayElement;
        dw.Type = type;
        dw.WriteType = DSWriteType::TextureView;
        dw.TextureLayout = ImageLayout::Undefined;
        dw.TextureView = texView;
        dw.Sampler = samp;

        descriptorWrites.push_back(dw);

        return *this;
    }

    DescriptorSetUpdater& DescriptorSetUpdater::AddBuffer(uint32_t binding, uint32_t arrayElement, DescriptorType type, Buffer* buf)
    {
        DSWrite dw{};
        dw.Binding = binding;
        dw.ArrayElement = arrayElement;
        dw.Type = type;
        dw.WriteType = DSWriteType::Buffer;
        dw.Buffer = buf;

        descriptorWrites.push_back(dw);

        return *this;
    }

    void DescriptorSetUpdater::Update()
    {
        std::vector<VkWriteDescriptorSet> writes;
        std::vector<VkDescriptorImageInfo> imageInfos;
        std::vector<VkDescriptorBufferInfo> bufferInfos;

        int numImageInfos = 0;
        int numBufferInfos = 0;

        for (DSWrite& dw : descriptorWrites)
        {
            if (dw.WriteType == DSWriteType::Texture || dw.WriteType == DSWriteType::TextureView)
            {
                numImageInfos++;
            }
            else if (dw.WriteType == DSWriteType::Buffer)
            {
                numBufferInfos++;
            }
        }

        imageInfos.reserve(numImageInfos);
        bufferInfos.reserve(numBufferInfos);

        for (DSWrite& dw : descriptorWrites)
        {
            VkWriteDescriptorSet vw{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            vw.dstSet = ds->GetNativeHandle();
            vw.dstBinding = dw.Binding;
            vw.dstArrayElement = dw.ArrayElement;
            vw.descriptorCount = 1;
            vw.descriptorType = (VkDescriptorType)dw.Type;
            
            switch (dw.WriteType)
            {
                case DSWriteType::Texture:
                case DSWriteType::TextureView:
                {
                    VkDescriptorImageInfo dii{};
                    if (dw.TextureLayout == ImageLayout::Undefined)
                    {
                        if (dw.Type != DescriptorType::StorageImage)
                            dii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                        else
                            dii.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                    }
                    else
                    {
                        dii.imageLayout = (VkImageLayout)dw.TextureLayout;
                    }

                    if (dw.WriteType == DSWriteType::Texture)
                        dii.imageView = dw.Texture->GetView();
                    else
                        dii.imageView = dw.TextureView->GetNativeHandle();

                    if (dw.Sampler != nullptr)
                        dii.sampler = dw.Sampler->GetNativeHandle();

                    imageInfos.push_back(dii);
                    vw.pImageInfo = &imageInfos[imageInfos.size() - 1];
                    break;
                }
                case DSWriteType::Buffer:
                {
                    VkDescriptorBufferInfo bii{};
                    bii.buffer = dw.Buffer->GetNativeHandle();
                    bii.offset = 0;
                    bii.range = VK_WHOLE_SIZE;
                    bufferInfos.push_back(bii);
                    vw.pBufferInfo = &bufferInfos[bufferInfos.size() - 1];
                    break;
                }
            }

            writes.push_back(vw);
        }

        vkUpdateDescriptorSets(handles->Device, writes.size(), writes.data(), 0, nullptr);
    }
}