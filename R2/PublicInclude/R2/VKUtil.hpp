#pragma once
#include <stdint.h>

#define VK_DEFINE_HANDLE(object) typedef struct object##_T* object;
VK_DEFINE_HANDLE(VkCommandBuffer)
#undef VK_DEFINE_HANDLE

namespace R2::VK
{
    struct Handles;

    class Utils
    {
    public:
        static void SetupImmediateCommandBuffer(const Handles* handles);
        static VkCommandBuffer AcquireImmediateCommandBuffer();
        static void ExecuteImmediateCommandBuffer();
    private:
        static const Handles* handles;
        static VkCommandBuffer immediateCommandBuffer;
    };

    enum class AccessFlags : uint64_t;
    enum class PipelineStageFlags : uint64_t;
    PipelineStageFlags getPipelineStage(AccessFlags access);

    inline int mipScale(int val, int mip)
    {
        int scaled = val >> mip;
        return scaled > 1 ? scaled : 1;
    }

    inline uint32_t mipScale(uint32_t val, uint32_t mip)
    {
        uint32_t scaled = val >> mip;
        return scaled > 1 ? scaled : 1;
    }
}