#include <R2/VKBuffer.hpp>
#include <R2/VKCore.hpp>
#include <R2/VKCommandBuffer.hpp>
#include <R2/VKDeletionQueue.hpp>
#include <R2/VKEnums.hpp>
#include <R2/VKUtil.hpp>
#include <volk.h>
#include <vk_mem_alloc.h>

namespace R2::VK
{
    bool hasUsage(BufferUsage usages, BufferUsage flag)
    {
        return ((uint32_t)usages & (uint32_t)flag) == (uint32_t)flag;
    }

    VkBufferUsageFlags convertUsages(BufferUsage usage)
    {
        VkBufferUsageFlags flags = 0;

        if (hasUsage(usage, BufferUsage::Storage))
            flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        
        if (hasUsage(usage, BufferUsage::Uniform))
            flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

        if (hasUsage(usage, BufferUsage::Index))
            flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

        if (hasUsage(usage, BufferUsage::Vertex))
            flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

        if (hasUsage(usage, BufferUsage::Indirect))
            flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

        return flags;
    }

    Buffer::Buffer(Core* renderer, const BufferCreateInfo& createInfo)
        : renderer(renderer)
        , lastAccess(AccessFlags::HostWrite)
        , lastPipelineStage(PipelineStageFlags::Host)
    {
        size = createInfo.Size;
        usage = createInfo.Usage;

        VkBufferCreateInfo bci{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bci.size = createInfo.Size;
        bci.usage = convertUsages(createInfo.Usage);
        bci.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bci.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo vaci{};
        vaci.usage = VMA_MEMORY_USAGE_AUTO;

        if (createInfo.Mappable)
        {
            vaci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        }

        VKCHECK(vmaCreateBuffer(renderer->handles.Allocator, &bci, &vaci, &buffer, &allocation, nullptr));
    }

    VkBuffer Buffer::GetNativeHandle()
    {
        return buffer;
    }

    size_t Buffer::GetSize()
    {
        return size;
    }

    BufferUsage Buffer::GetUsage()
    {
        return usage;
    }

    void* Buffer::Map()
    {
        void* mem;
        VKCHECK(vmaMapMemory(renderer->handles.Allocator, allocation, &mem));

        return mem;
    }

    void Buffer::Unmap()
    {
        vmaUnmapMemory(renderer->handles.Allocator, allocation);
    }

    void Buffer::CopyTo(VkCommandBuffer cb, Buffer* other, uint64_t numBytes, uint64_t srcOffset, uint64_t dstOffset)
    {
        VkBufferCopy bufferCopy{};
        bufferCopy.dstOffset = dstOffset;
        bufferCopy.srcOffset = srcOffset;
        bufferCopy.size = numBytes;
        vkCmdCopyBuffer(cb, buffer, other->buffer, 1, &bufferCopy);
    }

    void Buffer::Acquire(CommandBuffer cb, AccessFlags access)
    {
        VkBufferMemoryBarrier2 bmb { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2 };
        bmb.buffer = buffer;
        bmb.offset = 0;
        bmb.size = VK_WHOLE_SIZE;
        bmb.srcAccessMask = (VkAccessFlags2)lastAccess;
        bmb.srcStageMask = (VkPipelineStageFlags2)lastPipelineStage;
        bmb.dstAccessMask = (VkAccessFlags2)access;
        bmb.dstStageMask = (VkPipelineStageFlags2)getPipelineStage(access);

        VkDependencyInfo di { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        di.pBufferMemoryBarriers = &bmb;
        di.bufferMemoryBarrierCount = 1;
        vkCmdPipelineBarrier2(cb.GetNativeHandle(), &di);
        lastAccess = access;
        lastPipelineStage = getPipelineStage(access);
    }

    void Buffer::Acquire(CommandBuffer cb, AccessFlags access, PipelineStageFlags stage)
    {
        VkBufferMemoryBarrier2 bmb { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2 };
        bmb.buffer = buffer;
        bmb.offset = 0;
        bmb.size = VK_WHOLE_SIZE;
        bmb.srcAccessMask = (VkAccessFlags2)lastAccess;
        bmb.srcStageMask = (VkPipelineStageFlags2)lastPipelineStage;
        bmb.dstAccessMask = (VkAccessFlags2)access;
        bmb.dstStageMask = (VkPipelineStageFlags2)stage;

        VkDependencyInfo di { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        di.pBufferMemoryBarriers = &bmb;
        di.bufferMemoryBarrierCount = 1;
        vkCmdPipelineBarrier2(cb.GetNativeHandle(), &di);
        lastAccess = access;
        lastPipelineStage = stage;
    }

    Buffer::~Buffer()
    {
        DeletionQueue* dq = renderer->perFrameResources[renderer->frameIndex].DeletionQueue;
        dq->QueueObjectDeletion(buffer, VK_OBJECT_TYPE_BUFFER);
        dq->QueueMemoryFree(allocation);
    }
}