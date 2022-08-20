#pragma once
#include <stdint.h>
#include <mutex>

#define VK_DEFINE_HANDLE(object) typedef struct object##_T* object;
VK_DEFINE_HANDLE(VmaVirtualBlock)

namespace R2
{
    namespace VK
    {
        class Buffer;
        class Core;
        struct BufferCreateInfo;
    }

    VK_DEFINE_HANDLE(SubAllocationHandle)

    class SubAllocatedBuffer
    {
        VK::Buffer* buf;
        VmaVirtualBlock virtualBlock;
        std::mutex mutex;
    public:
        SubAllocatedBuffer(VK::Core* core, const VK::BufferCreateInfo& ci);
        ~SubAllocatedBuffer();
        VK::Buffer* GetBuffer();
        size_t Allocate(size_t amount, SubAllocationHandle& allocation);
        void Free(SubAllocationHandle allocation);
    };
}
#undef VK_DEFINE_HANDLE