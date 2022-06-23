#include <R2/VKDeletionQueue.hpp>
#include <R2/VKCore.hpp>
#include <volk.h>
#include <vk_mem_alloc.h>
#include <assert.h>

namespace R2::VK
{
    DeletionQueue::DeletionQueue(const Handles* handles)
        : handles(handles)
    {
    }

    void DeletionQueue::QueueObjectDeletion(void* object, VkObjectType type)
    {
        objectDeletions.emplace_back(object, type);
    }

    void DeletionQueue::QueueMemoryFree(VmaAllocation allocation)
    {
        memoryFrees.emplace_back(allocation);
    }

    void DeletionQueue::QueueDescriptorSetFree(VkDescriptorPool pool, VkDescriptorSet set)
    {
        dsFrees.emplace_back(pool, set);
    }

    void DeletionQueue::Cleanup()
    {
        for (const ObjectDeletion& od : objectDeletions)
        {
            processObjectDeletion(od);
        }

        for (const MemoryFree& mf : memoryFrees)
        {
            processMemoryFree(mf);
        }

        for (const DescriptorSetFree& dsf : dsFrees)
        {
            vkFreeDescriptorSets(handles->Device, dsf.desciptorPool, 1, &dsf.descriptorSet);
        }

        objectDeletions.clear();
        memoryFrees.clear();
        dsFrees.clear();
    }

    void DeletionQueue::processObjectDeletion(const ObjectDeletion& od)
    {
        void* object = od.object;
        switch (od.type)
        {
        case VK_OBJECT_TYPE_EVENT:
            vkDestroyEvent(handles->Device, (VkEvent)object, handles->AllocCallbacks);
            break;
        case VK_OBJECT_TYPE_PIPELINE:
            vkDestroyPipeline(handles->Device, (VkPipeline)object, handles->AllocCallbacks);
            break;
        case VK_OBJECT_TYPE_SAMPLER:
            vkDestroySampler(handles->Device, (VkSampler)object, handles->AllocCallbacks);
            break;
        case VK_OBJECT_TYPE_PIPELINE_LAYOUT:
            vkDestroyPipelineLayout(handles->Device, (VkPipelineLayout)object, handles->AllocCallbacks);
            break;
        case VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT:
            vkDestroyDescriptorSetLayout(handles->Device, (VkDescriptorSetLayout)object, handles->AllocCallbacks);
            break;
        case VK_OBJECT_TYPE_SHADER_MODULE:
            vkDestroyShaderModule(handles->Device, (VkShaderModule)object, handles->AllocCallbacks);
            break;
        case VK_OBJECT_TYPE_RENDER_PASS:
            vkDestroyRenderPass(handles->Device, (VkRenderPass)object, handles->AllocCallbacks);
            break;
        case VK_OBJECT_TYPE_FRAMEBUFFER:
            vkDestroyFramebuffer(handles->Device, (VkFramebuffer)object, handles->AllocCallbacks);
            break;
        case VK_OBJECT_TYPE_DESCRIPTOR_POOL:
            vkDestroyDescriptorPool(handles->Device, (VkDescriptorPool)object, handles->AllocCallbacks);
            break;
        case VK_OBJECT_TYPE_BUFFER:
            vkDestroyBuffer(handles->Device, (VkBuffer)object, handles->AllocCallbacks);
            break;
        case VK_OBJECT_TYPE_IMAGE:
            vkDestroyImage(handles->Device, (VkImage)object, handles->AllocCallbacks);
            break;
        case VK_OBJECT_TYPE_IMAGE_VIEW:
            vkDestroyImageView(handles->Device, (VkImageView)object, handles->AllocCallbacks);
            break;
        default:
            assert(false && "Unhandled Vulkan object deletion! This is a bug.");
            break;
        }
    }

    void DeletionQueue::processMemoryFree(const MemoryFree& mf)
    {
        vmaFreeMemory(handles->Allocator, mf.allocation);
    }
}