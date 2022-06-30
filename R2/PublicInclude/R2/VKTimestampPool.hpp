#pragma once

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
        void Reset();
        void Reset(int offset, int count);
        void WriteTimestamp(CommandBuffer& cb, int index);
    };
}