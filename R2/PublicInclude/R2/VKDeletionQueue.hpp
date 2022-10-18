#pragma once
#include <stdint.h>
#include <vector>

#define VK_DEFINE_HANDLE(object) typedef struct object##_T* object;
VK_DEFINE_HANDLE(VmaAllocator)
VK_DEFINE_HANDLE(VmaAllocation)
VK_DEFINE_HANDLE(VkDevice)
VK_DEFINE_HANDLE(VkDescriptorPool)
VK_DEFINE_HANDLE(VkDescriptorSet)
#undef VK_DEFINE_HANDLE

namespace R2::VK
{
    struct Handles;

    class DeletionQueue
    {
    public:
        DeletionQueue(const Handles* handles);
        void QueueObjectDeletion(void* object, uint32_t type);
        void QueueMemoryFree(VmaAllocation allocation);
        void QueueDescriptorSetFree(VkDescriptorPool dPool, VkDescriptorSet ds);
        void Cleanup();
    private:
        const Handles* handles;

        struct ObjectDeletion
        {
            void* object;
            uint32_t type;
        };

        struct MemoryFree
        {
            VmaAllocation allocation;
        };

        struct DescriptorSetFree
        {
            VkDescriptorPool desciptorPool;
            VkDescriptorSet descriptorSet;
        };


        std::vector<ObjectDeletion> objectDeletions;
        std::vector<MemoryFree> memoryFrees;
        std::vector<DescriptorSetFree> dsFrees;

        void processObjectDeletion(const ObjectDeletion& od);
        void processMemoryFree(const MemoryFree& mf);
    };
}