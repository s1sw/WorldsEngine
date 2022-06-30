#include <R2/VKTimestampPool.hpp>
#include <volk.h>
#include <R2/VKCore.hpp>
#include <R2/VKCommandBuffer.hpp>

namespace R2::VK
{
    TimestampPool::TimestampPool(const Handles* handles, int numTimestamps)
        : handles(handles)
        , numTimestamps(numTimestamps)
    {
        VkQueryPoolCreateInfo qpci{ VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
        qpci.queryType = VK_QUERY_TYPE_TIMESTAMP;
        qpci.queryCount = numTimestamps;
        VKCHECK(vkCreateQueryPool(handles->Device, &qpci, handles->AllocCallbacks, &queryPool));
    }

    void TimestampPool::Reset()
    {
        vkResetQueryPool(handles->Device, queryPool, 0, numTimestamps);
    }

    void TimestampPool::Reset(int offset, int count)
    {
        vkResetQueryPool(handles->Device, queryPool, offset, count);
    }

    void TimestampPool::WriteTimestamp(CommandBuffer& cb, int index)
    {
        vkCmdWriteTimestamp2(cb.GetNativeHandle(), VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, queryPool, index);
    }
}