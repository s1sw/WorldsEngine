#include "RenderInternal.hpp"

namespace worlds {
    std::vector<DeletionQueue::DQueue> DeletionQueue::deletionQueues;
    uint32_t DeletionQueue::currentFrameIndex = 0u;
    VkDevice DeletionQueue::deletionDevice = VK_NULL_HANDLE;
    VmaAllocator DeletionQueue::deletionAllocator = nullptr;

    void DeletionQueue::queueObjectDeletion(void* object, VkObjectType type) {
        deletionQueues[currentFrameIndex].objectDeletions.emplace_back(object, type);

    }

    void DeletionQueue::queueMemoryFree(VmaAllocation allocation) {
        deletionQueues[currentFrameIndex].memoryFrees.emplace_back(allocation);
    }

    void DeletionQueue::queueDescriptorSetFree(VkDescriptorPool pool, VkDescriptorSet set) {
        deletionQueues[currentFrameIndex].dsFrees.emplace_back(pool, set);
    }

    void DeletionQueue::setCurrentFrame(uint32_t frame) {
        currentFrameIndex = frame;
    }

    void DeletionQueue::cleanupFrame(uint32_t frame) {
        DQueue& queue = deletionQueues[currentFrameIndex];

        for (const ObjectDeletion& od : queue.objectDeletions) {
            processObjectDeletion(od);
        }

        for (const MemoryFree& mf : queue.memoryFrees) {
            processMemoryFree(mf);
        }

        for (const DescriptorSetFree& dsf : queue.dsFrees) {
            vkFreeDescriptorSets(deletionDevice, dsf.desciptorPool, 1, &dsf.descriptorSet);
        }

        queue.objectDeletions.clear();
        queue.memoryFrees.clear();
        queue.dsFrees.clear();
    }

    void DeletionQueue::resize(uint32_t maxFrames) {
        deletionQueues.resize(maxFrames);
    }

    void DeletionQueue::processObjectDeletion(const ObjectDeletion& od) {
        void* object = od.object;
        switch (od.type) {
        case VK_OBJECT_TYPE_EVENT:
            vkDestroyEvent(deletionDevice, (VkEvent)object, nullptr);
            break;
        case VK_OBJECT_TYPE_PIPELINE:
            vkDestroyPipeline(deletionDevice, (VkPipeline)object, nullptr);
            break;
        case VK_OBJECT_TYPE_SAMPLER:
            vkDestroySampler(deletionDevice, (VkSampler)object, nullptr);
            break;
        case VK_OBJECT_TYPE_PIPELINE_LAYOUT:
            vkDestroyPipelineLayout(deletionDevice, (VkPipelineLayout)object, nullptr);
            break;
        case VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT:
            vkDestroyDescriptorSetLayout(deletionDevice, (VkDescriptorSetLayout)object, nullptr);
            break;
        case VK_OBJECT_TYPE_SHADER_MODULE:
            vkDestroyShaderModule(deletionDevice, (VkShaderModule)object, nullptr);
            break;
        case VK_OBJECT_TYPE_RENDER_PASS:
            vkDestroyRenderPass(deletionDevice, (VkRenderPass)object, nullptr);
            break;
        case VK_OBJECT_TYPE_FRAMEBUFFER:
            vkDestroyFramebuffer(deletionDevice, (VkFramebuffer)object, nullptr);
            break;
        case VK_OBJECT_TYPE_DESCRIPTOR_POOL:
            vkDestroyDescriptorPool(deletionDevice, (VkDescriptorPool)object, nullptr);
            break;
        case VK_OBJECT_TYPE_BUFFER:
            vkDestroyBuffer(deletionDevice, (VkBuffer)object, nullptr);
            break;
        case VK_OBJECT_TYPE_IMAGE:
            vkDestroyImage(deletionDevice, (VkImage)object, nullptr);
            break;
        case VK_OBJECT_TYPE_IMAGE_VIEW:
            vkDestroyImageView(deletionDevice, (VkImageView)object, nullptr);
            break;
        default:
            fatalErr("Unhandled Vulkan object deletion! This is a bug.");
            break;
        }
    }

    void DeletionQueue::processMemoryFree(const MemoryFree& mf) {
        vmaFreeMemory(deletionAllocator, mf.allocation);
    }

    void DeletionQueue::intitialize(VkDevice device, VmaAllocator allocator) {
        deletionDevice = device;
        deletionAllocator = allocator;
    }
}
