#pragma once
#include "vku.hpp"

namespace vku {
    /// Convenience class for updating descriptor sets (uniforms)
    class DescriptorSetUpdater {
    public:
        DescriptorSetUpdater(int maxBuffers = 10, int maxImages = 10, int maxBufferViews = 0) {
            // we must pre-size these buffers as we take pointers to their members.
            bufferInfo_.resize(maxBuffers);
            imageInfo_.resize(maxImages);
            bufferViews_.resize(maxBufferViews);
        }

        /// Call this to begin a new descriptor set.
        void beginDescriptorSet(VkDescriptorSet dstSet) {
            dstSet_ = dstSet;
        }

        /// Call this to begin a new set of images.
        void beginImages(uint32_t dstBinding, uint32_t dstArrayElement, VkDescriptorType descriptorType) {
            VkWriteDescriptorSet wdesc{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            wdesc.dstSet = dstSet_;
            wdesc.dstBinding = dstBinding;
            wdesc.dstArrayElement = dstArrayElement;
            wdesc.descriptorCount = 0;
            wdesc.descriptorType = descriptorType;
            wdesc.pImageInfo = imageInfo_.data() + numImages_;
            descriptorWrites_.push_back(wdesc);
        }

        /// Call this to add a combined image sampler.
        void image(VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout) {
            if (!descriptorWrites_.empty() && (size_t)numImages_ != imageInfo_.size() && descriptorWrites_.back().pImageInfo) {
                descriptorWrites_.back().descriptorCount++;
                imageInfo_[numImages_++] = VkDescriptorImageInfo{ sampler, imageView, imageLayout };
            } else {
                ok_ = false;
            }
        }

        /// Call this to start defining buffers.
        void beginBuffers(uint32_t dstBinding, uint32_t dstArrayElement, VkDescriptorType descriptorType) {
            VkWriteDescriptorSet wdesc{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            wdesc.dstSet = dstSet_;
            wdesc.dstBinding = dstBinding;
            wdesc.dstArrayElement = dstArrayElement;
            wdesc.descriptorCount = 0;
            wdesc.descriptorType = descriptorType;
            wdesc.pBufferInfo = bufferInfo_.data() + numBuffers_;
            descriptorWrites_.push_back(wdesc);
        }

        /// Call this to add a new buffer.
        void buffer(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range) {
            if (!descriptorWrites_.empty() && (size_t)numBuffers_ != bufferInfo_.size() && descriptorWrites_.back().pBufferInfo) {
                descriptorWrites_.back().descriptorCount++;
                bufferInfo_[numBuffers_++] = VkDescriptorBufferInfo{ buffer, offset, range };
            } else {
                ok_ = false;
            }
        }

        /// Call this to start adding buffer views. (for example, writable images).
        void beginBufferViews(uint32_t dstBinding, uint32_t dstArrayElement, VkDescriptorType descriptorType) {
            VkWriteDescriptorSet wdesc{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            wdesc.dstSet = dstSet_;
            wdesc.dstBinding = dstBinding;
            wdesc.dstArrayElement = dstArrayElement;
            wdesc.descriptorCount = 0;
            wdesc.descriptorType = descriptorType;
            wdesc.pTexelBufferView = bufferViews_.data() + numBufferViews_;
            descriptorWrites_.push_back(wdesc);
        }

        /// Call this to add a buffer view. (Texel images)
        void bufferView(VkBufferView view) {
            if (!descriptorWrites_.empty() && (size_t)numBufferViews_ != bufferViews_.size() && descriptorWrites_.back().pImageInfo) {
                descriptorWrites_.back().descriptorCount++;
                bufferViews_[numBufferViews_++] = view;
            } else {
                ok_ = false;
            }
        }

        /// Copy an existing descriptor.
        void copy(VkDescriptorSet srcSet, uint32_t srcBinding, uint32_t srcArrayElement, VkDescriptorSet dstSet, uint32_t dstBinding, uint32_t dstArrayElement, uint32_t descriptorCount) {
            descriptorCopies_.emplace_back(VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET, nullptr, srcSet, srcBinding, srcArrayElement, dstSet, dstBinding, dstArrayElement, descriptorCount);
        }

        /// Call this to update the descriptor sets with their pointers (but not data).
        void update(const VkDevice& device) const {
            vkUpdateDescriptorSets(device, descriptorWrites_.size(), descriptorWrites_.data(), descriptorCopies_.size(), descriptorCopies_.data());
        }

        /// Returns true if the updater is error free.
        bool ok() const { return ok_; }
    private:
        std::vector<VkDescriptorBufferInfo> bufferInfo_;
        std::vector<VkDescriptorImageInfo> imageInfo_;
        std::vector<VkWriteDescriptorSet> descriptorWrites_;
        std::vector<VkCopyDescriptorSet> descriptorCopies_;
        std::vector<VkBufferView> bufferViews_;
        VkDescriptorSet dstSet_;
        int numBuffers_ = 0;
        int numImages_ = 0;
        int numBufferViews_ = 0;
        bool ok_ = true;
    };

    /// A factory class for descriptor set layouts. (An interface to the shaders)
    class DescriptorSetLayoutMaker {
    public:
        DescriptorSetLayoutMaker() {
        }

        void buffer(uint32_t binding, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags, uint32_t descriptorCount) {
            s.bindings.emplace_back(binding, descriptorType, descriptorCount, stageFlags, nullptr);
            s.bindFlags.push_back({});
        }

        void image(uint32_t binding, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags, uint32_t descriptorCount) {
            s.bindings.emplace_back(binding, descriptorType, descriptorCount, stageFlags, nullptr);
            s.bindFlags.push_back({});
        }

        void samplers(uint32_t binding, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags, const std::vector<VkSampler> immutableSamplers) {
            s.samplers.push_back(immutableSamplers);
            auto pImmutableSamplers = s.samplers.back().data();
            s.bindings.emplace_back(binding, descriptorType, (uint32_t)immutableSamplers.size(), stageFlags, pImmutableSamplers);
            s.bindFlags.push_back({});
        }

        void bufferView(uint32_t binding, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags, uint32_t descriptorCount) {
            s.bindings.emplace_back(binding, descriptorType, descriptorCount, stageFlags, nullptr);
            s.bindFlags.push_back({});
        }

        void bindFlag(uint32_t binding, VkDescriptorBindingFlags flags) {
            s.bindFlags[binding] = flags;
            s.useBindFlags = true;
        }

        /// Create a self-deleting descriptor set object.
        VkDescriptorSetLayout create(VkDevice device) const {
            VkDescriptorSetLayoutCreateInfo dsci{};
            dsci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            dsci.bindingCount = (uint32_t)s.bindings.size();
            dsci.pBindings = s.bindings.data();

            VkDescriptorSetLayoutBindingFlagsCreateInfo dslbfci{};
            if (s.useBindFlags) {
                dslbfci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
                dslbfci.bindingCount = (uint32_t)s.bindings.size();
                dslbfci.pBindingFlags = s.bindFlags.data();
                dsci.pNext = &dslbfci;
            }

            VkDescriptorSetLayout layout;
            VKCHECK(vkCreateDescriptorSetLayout(device, &dsci, nullptr, &layout));
            return layout;
        }

    private:
        struct State {
            std::vector<VkDescriptorSetLayoutBinding> bindings;
            std::vector<std::vector<VkSampler> > samplers;
            int numSamplers = 0;
            bool useBindFlags = false;
            std::vector<VkDescriptorBindingFlags> bindFlags;
        };

        State s;
    };

    /// A factory class for descriptor sets (A set of uniform bindings)
    class DescriptorSetMaker {
    public:
        // Construct a new, empty DescriptorSetMaker.
        DescriptorSetMaker() {
        }

        /// Add another layout describing a descriptor set.
        void layout(VkDescriptorSetLayout layout) {
            s.layouts.push_back(layout);
        }

        /// Allocate a vector of non-self-deleting descriptor sets
        /// Note: descriptor sets get freed with the pool, so this is the better choice.
        std::vector<VkDescriptorSet> create(VkDevice device, VkDescriptorPool descriptorPool) const {
            VkDescriptorSetAllocateInfo dsai{};
            dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            dsai.descriptorPool = descriptorPool;
            dsai.descriptorSetCount = (uint32_t)s.layouts.size();
            dsai.pSetLayouts = s.layouts.data();

            std::vector<VkDescriptorSet> descriptorSets;
            descriptorSets.resize(s.layouts.size());

            VKCHECK(vkAllocateDescriptorSets(device, &dsai, descriptorSets.data()));

            return descriptorSets;
        }

    private:
        struct State {
            std::vector<VkDescriptorSetLayout> layouts;
        };

        State s;
    };
}
