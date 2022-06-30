#include <R2/VKDescriptorSet.hpp>
#include <R2/VKTexture.hpp>
#include <R2/VKBuffer.hpp>
#include <R2/VKCore.hpp>
#include <R2/VKSampler.hpp>
#include <volk.h>

namespace R2::VK
{
    DescriptorSet::DescriptorSet(const Handles* handles, VkDescriptorSet set)
        : handles(handles)
        , set(set)
    {}

    VkDescriptorSet DescriptorSet::GetNativeHandle()
    {
        return set;
    }

    DescriptorSet::~DescriptorSet()
    {

    }

    DescriptorSetLayout::DescriptorSetLayout(const Handles* handles, VkDescriptorSetLayout layout)
        : handles(handles)
        , layout(layout)
    {}

    VkDescriptorSetLayout DescriptorSetLayout::GetNativeHandle()
    {
        return layout;
    }

    DescriptorSetLayout::~DescriptorSetLayout()
    {
        vkDestroyDescriptorSetLayout(handles->Device, layout, handles->AllocCallbacks);
    }

    DescriptorSetLayoutBuilder::DescriptorSetLayoutBuilder(const Handles* handles)
        : handles(handles)
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
        bindings.front().PartiallyBound = true;
        return *this;
    }

    DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::UpdateAfterBind()
    {
        bindings.front().UpdateAfterBind = true;
        return *this;
    }

    DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::VariableDescriptorCount()
    {
        bindings.front().VariableDescriptorCount = true;
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

        VkDescriptorSetLayout dsl;
        VKCHECK(vkCreateDescriptorSetLayout(handles->Device, &dslci, handles->AllocCallbacks, &dsl));

        return new DescriptorSetLayout(handles, dsl);
    }

    DescriptorSetUpdater::DescriptorSetUpdater(const Handles* handles, DescriptorSet* ds)
        : handles(handles)
        , ds(ds)
    {}

    DescriptorSetUpdater& DescriptorSetUpdater::AddTexture(uint32_t binding, uint32_t arrayElement, DescriptorType type, Texture* tex, Sampler* samp)
    {
        DSWrite dw{};
        dw.Binding = binding;
        dw.ArrayElement = arrayElement;
        dw.Type = type;
        dw.Texture = tex;
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
            if (dw.Texture)
            {
                numImageInfos++;
            }
            else if (dw.Buffer)
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
            
            if (dw.Texture)
            {
                VkDescriptorImageInfo dii{};
                dii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                dii.imageView = dw.Texture->GetView();
                dii.sampler = dw.Sampler->GetNativeHandle();
                imageInfos.push_back(dii);
                vw.pImageInfo = &imageInfos[imageInfos.size() - 1];
            }
            else if (dw.Buffer)
            {
                VkDescriptorBufferInfo bii{};
                bii.buffer = dw.Buffer->GetNativeHandle();
                bii.offset = 0;
                bii.range = VK_WHOLE_SIZE;
                bufferInfos.push_back(bii);
                vw.pBufferInfo = &bufferInfos[imageInfos.size() - 1];
            }

            writes.push_back(vw);
        }

        vkUpdateDescriptorSets(handles->Device, writes.size(), writes.data(), 0, nullptr);
    }
}