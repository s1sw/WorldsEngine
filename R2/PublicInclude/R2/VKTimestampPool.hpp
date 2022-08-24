#pragma once
#include <stdint.h>

#define VK_DEFINE_HANDLE(object) typedef struct object##_T* object;
VK_DEFINE_HANDLE(VkQueryPool)
#undef VK_DEFINE_HANDLE

namespace R2::VK
{
    class CommandBuffer;
    struct Handles;

    class TimestampPool
    {
        const Handles* handles;
        VkQueryPool queryPool;
        int numTimestamps;
    public:
        TimestampPool(const Handles* handles, int numTimestamps);
        void Reset(CommandBuffer& cb);
        void Reset(CommandBuffer& cb, int offset, int count);
        bool GetTimestamps(int offset, int count, uint64_t* out);
        void WriteTimestamp(CommandBuffer& cb, int index);
    };
}