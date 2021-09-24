#include "vku.hpp"

namespace vku {
    /// A generic buffer that may be used as a vertex buffer, uniform buffer or other kinds of memory resident data.
    /// Buffers require memory objects which represent GPU and CPU resources.
    class GenericBuffer {
    public:
        GenericBuffer() : buffer_(nullptr) {
        }

        GenericBuffer(VkDevice device, VmaAllocator allocator, VkBufferUsageFlags usage, VkDeviceSize size, VmaMemoryUsage memUsage = VMA_MEMORY_USAGE_GPU_ONLY, const char* debugName = nullptr) : debugName(debugName) {
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

            VkResult bufferCreateResult = vmaCreateBuffer(allocator, &ci, &allocInfo, &buffer_, &allocation, nullptr);
            if (bufferCreateResult != VK_SUCCESS) {
                fatalErr("error while creating buffer");
            }

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

        /// For a host visible buffer, copy memory to the buffer object.
        void updateLocal(const VkDevice& device, const void* value, VkDeviceSize size) const {
            void* ptr; // = device.mapMemory(*mem_, 0, size_, VkMemoryMapFlags{});
            vmaMapMemory(allocator, allocation, &ptr);
            memcpy(ptr, value, (size_t)size);
            flush(device);
            vmaUnmapMemory(allocator, allocation);
        }

        /// For a purely device local buffer, copy memory to the buffer object immediately.
        /// Note that this will stall the pipeline!
        void upload(VkDevice device, VkCommandPool commandPool, VkQueue queue, const void* value, VkDeviceSize size) const {
            if (size == 0) return;
            auto tmp = vku::GenericBuffer(device, allocator, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, size, VMA_MEMORY_USAGE_CPU_ONLY);
            tmp.updateLocal(device, value, size);

            vku::executeImmediately(device, commandPool, queue, [&](VkCommandBuffer cb) {
                VkBufferCopy bc{ 0, 0, size };
                vkCmdCopyBuffer(cb, tmp.buffer(), buffer_, 1, &bc);
                });
        }

        template<typename T>
        void upload(VkDevice device, VkCommandPool commandPool, VkQueue queue, const std::vector<T>& value) const {
            upload(device, commandPool, queue, value.data(), value.size() * sizeof(T));
        }

        template<typename T>
        void upload(VkDevice device, VkCommandPool commandPool, VkQueue queue, const T& value) const {
            upload(device, commandPool, queue, &value, sizeof(value));
        }

        void barrier(VkCommandBuffer cb, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, uint32_t srcQueueFamilyIndex, uint32_t dstQueueFamilyIndex) const {
            VkBufferMemoryBarrier bmb{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr,
                srcAccessMask, dstAccessMask, srcQueueFamilyIndex, dstQueueFamilyIndex, buffer_, 0, size_ };

            vkCmdPipelineBarrier(cb, srcStageMask, dstStageMask, dependencyFlags, 0, nullptr, 1, &bmb, 0, nullptr);
        }

        template<class Type, class Allocator>
        void updateLocal(const VkDevice& device, const std::vector<Type, Allocator>& value) const {
            updateLocal(device, (void*)value.data(), VkDeviceSize(value.size() * sizeof(Type)));
        }

        template<class Type>
        void updateLocal(const VkDevice& device, const Type& value) const {
            updateLocal(device, (void*)&value, VkDeviceSize(sizeof(Type)));
        }

        void* map(const VkDevice& device) const {
            (void)device;
            void* data;
            vmaMapMemory(allocator, allocation, &data);
            return data;
        }

        void unmap(const VkDevice& device) const {
            (void)device;
            vmaUnmapMemory(allocator, allocation);
        }

        void flush(const VkDevice& device) const {
            (void)device;
            vmaFlushAllocation(allocator, allocation, 0, VK_WHOLE_SIZE);
        }

        void invalidate(const VkDevice& device) const {
            (void)device;
            vmaInvalidateAllocation(allocator, allocation, 0, VK_WHOLE_SIZE);
        }

        GenericBuffer(GenericBuffer const&) = delete;

        GenericBuffer(GenericBuffer&& other) noexcept
            : buffer_(other.buffer_)
            , size_(other.size_)
            , debugName(other.debugName)
            , allocation(other.allocation)
            , allocator(other.allocator) {
            other.buffer_ = nullptr;
        }

        GenericBuffer& operator=(GenericBuffer&& other) noexcept {
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

        VkBuffer buffer() const { return buffer_; }
        VkDeviceSize size() const { return size_; }

        void destroy() {
            VkBuffer cBuf = buffer_;
            if (cBuf) {
                vmaDestroyBuffer(allocator, cBuf, allocation);
                buffer_ = VkBuffer{};
            }
        }

        ~GenericBuffer() {
            VkBuffer cBuf = buffer_;
            if (cBuf) {
                vmaDestroyBuffer(allocator, cBuf, allocation);
                buffer_ = VkBuffer{};
            }
        }

    private:
        VkBuffer buffer_;
        VkDeviceSize size_;
        const char* debugName;
        VmaAllocation allocation;
        VmaAllocator allocator;
    };

    /// This class is a specialisation of GenericBuffer for high performance vertex buffers on the GPU.
    /// You must upload the contents before use.
    class VertexBuffer : public GenericBuffer {
    public:
        VertexBuffer() {
        }

        VertexBuffer(const VkDevice& device, VmaAllocator allocator, size_t size, const char* debugName = nullptr)
            : GenericBuffer(device, allocator, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, size, VMA_MEMORY_USAGE_GPU_ONLY, debugName) {
        }
    };

    /// This class is a specialisation of GenericBuffer for low performance vertex buffers on the host.
    class HostVertexBuffer : public GenericBuffer {
    public:
        HostVertexBuffer() {
        }

        template<class Type, class Allocator>
        HostVertexBuffer(const VkDevice& device, VmaAllocator allocator, const std::vector<Type, Allocator>& value)
            : GenericBuffer(device, allocator, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, value.size() * sizeof(Type), VMA_MEMORY_USAGE_CPU_ONLY) {
            updateLocal(device, value);
        }
    };

    /// This class is a specialisation of GenericBuffer for high performance index buffers.
    /// You must upload the contents before use.
    class IndexBuffer : public GenericBuffer {
    public:
        IndexBuffer() {
        }

        IndexBuffer(const VkDevice& device, VmaAllocator allocator, size_t size, const char* debugName = nullptr)
            : GenericBuffer(device, allocator, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, size, VMA_MEMORY_USAGE_GPU_ONLY, debugName) {
        }
    };

    /// This class is a specialisation of GenericBuffer for low performance vertex buffers in CPU memory.
    class HostIndexBuffer : public GenericBuffer {
    public:
        HostIndexBuffer() {
        }

        template<class Type, class Allocator>
        HostIndexBuffer(const VkDevice& device, VmaAllocator allocator, const std::vector<Type, Allocator>& value)
            : GenericBuffer(device, allocator, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, value.size() * sizeof(Type), VMA_MEMORY_USAGE_CPU_ONLY) {
            updateLocal(device, value);
        }
    };

    /// This class is a specialisation of GenericBuffer for uniform buffers.
    class UniformBuffer : public GenericBuffer {
    public:
        UniformBuffer() {
        }

        /// Device local uniform buffer.
        UniformBuffer(const VkDevice& device, VmaAllocator allocator, size_t size, VmaMemoryUsage memUsage = VMA_MEMORY_USAGE_GPU_ONLY, const char* debugName = nullptr)
            : GenericBuffer(device, allocator, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, size, memUsage, debugName) {
        }
    };
}
