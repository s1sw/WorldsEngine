#include "vku.hpp"

namespace vku {
    GenericBuffer::GenericBuffer() : buffer_(VK_NULL_HANDLE) {}

    GenericBuffer::GenericBuffer(VkDevice device, VmaAllocator allocator, VkBufferUsageFlags usage, VkDeviceSize size, VmaMemoryUsage memUsage, const char* debugName) : debugName(debugName) {
        this->allocator = allocator;
        // Create the buffer object without memory.
        VkBufferCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        ci.size = size_ = size;
        ci.usage = usage;
        ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = memUsage;
        allocInfo.flags = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
        allocInfo.pUserData = (void*)debugName;

        VKCHECK(vmaCreateBuffer(allocator, &ci, &allocInfo, &buffer_, &allocation, nullptr));

        if (debugName) {
            VkDebugUtilsObjectNameInfoEXT nameInfo;
            nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
            nameInfo.pObjectName = debugName;
            nameInfo.objectHandle = (uint64_t)(VkBuffer)(buffer_);
            nameInfo.objectType = VK_OBJECT_TYPE_BUFFER;
            nameInfo.pNext = nullptr;
            auto setObjName = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT");
            setObjName(device, &nameInfo);
        }
    }

    GenericBuffer::GenericBuffer(GenericBuffer&& other) noexcept
        : buffer_(other.buffer_)
        , size_(other.size_)
        , debugName(other.debugName)
        , allocation(other.allocation)
        , allocator(other.allocator) {
        other.buffer_ = nullptr;
    }

    GenericBuffer& GenericBuffer::operator=(GenericBuffer&& other) noexcept {
        allocation = other.allocation;
        allocator = other.allocator;
        if (other.buffer_) {
            buffer_ = other.buffer_;
            other.buffer_ = nullptr;
        }
        size_ = other.size_;
        debugName = other.debugName;
        return *this;
    }

    void GenericBuffer::updateLocal(const VkDevice& device, const void* value, VkDeviceSize size) const {
        void* ptr; // = device.mapMemory(*mem_, 0, size_, VkMemoryMapFlags{});
        vmaMapMemory(allocator, allocation, &ptr);
        memcpy(ptr, value, (size_t)size);
        flush(device);
        vmaUnmapMemory(allocator, allocation);
    }

    void GenericBuffer::upload(VkDevice device, VkCommandPool commandPool, VkQueue queue, const void* value, VkDeviceSize size) const {
        if (size == 0) return;
        auto tmp = vku::GenericBuffer(device, allocator, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, size, VMA_MEMORY_USAGE_CPU_ONLY);
        tmp.updateLocal(device, value, size);

        vku::executeImmediately(device, commandPool, queue, [&](VkCommandBuffer cb) {
            VkBufferCopy bc{ 0, 0, size };
            vkCmdCopyBuffer(cb, tmp.buffer(), buffer_, 1, &bc);
            });
    }

    void GenericBuffer::barrier(VkCommandBuffer cb, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, uint32_t srcQueueFamilyIndex, uint32_t dstQueueFamilyIndex) const {
        VkBufferMemoryBarrier bmb{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr,
            srcAccessMask, dstAccessMask, srcQueueFamilyIndex, dstQueueFamilyIndex, buffer_, 0, size_ };

        vkCmdPipelineBarrier(cb, srcStageMask, dstStageMask, dependencyFlags, 0, nullptr, 1, &bmb, 0, nullptr);
    }

    void* GenericBuffer::map(const VkDevice& device) const {
        (void)device;
        void* data;
        vmaMapMemory(allocator, allocation, &data);
        return data;
    }

    void GenericBuffer::unmap(const VkDevice& device) const {
        (void)device;
        vmaUnmapMemory(allocator, allocation);
    }

    void GenericBuffer::flush(const VkDevice& device) const {
        (void)device;
        vmaFlushAllocation(allocator, allocation, 0, VK_WHOLE_SIZE);
    }

    void GenericBuffer::invalidate(const VkDevice& device) const {
        (void)device;
        vmaInvalidateAllocation(allocator, allocation, 0, VK_WHOLE_SIZE);
    }

    void GenericBuffer::destroy() {
        VkBuffer cBuf = buffer_;
        if (cBuf) {
            vmaDestroyBuffer(allocator, cBuf, allocation);
            buffer_ = VkBuffer{};
        }
    }

    GenericBuffer::~GenericBuffer() {
        VkBuffer cBuf = buffer_;
        if (cBuf) {
            vmaDestroyBuffer(allocator, cBuf, allocation);
            buffer_ = VkBuffer{};
        }
    }
}
