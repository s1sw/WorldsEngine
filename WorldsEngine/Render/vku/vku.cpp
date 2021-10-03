#include "vku.hpp"
#include <physfs.h>
#include <Core/AssetDB.hpp>

namespace vku {
    const char* toString(VkPhysicalDeviceType type) {
        switch (type) {
        case VK_PHYSICAL_DEVICE_TYPE_OTHER:
            return "Other";
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            return "Integrated GPU";
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            return "Discrete GPU";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            return "Virtual GPU";
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            return "CPU";
        default:
            return "Unknown";
        }
    }

    const char* toString(VkMemoryPropertyFlags flags) {
        static const char* props[] = {
            "Device Local",
            "Host Visible",
            "Host Coherent",
            "Host Cached",
            "Lazily Allocated",
            "Protected",
            "Device Coherent (AMD)",
            "Device Uncached (AMD)",
            "RDMA Capable (Nvidia)"
        };

        // Maximum length is (23 + 3) * 9 + 4
        //                   ^     ^    ^   ^
        //                   |     |    |   beginning and end braces + spaces
        //                   |     |    number of memory property types
        //                   |     spaces and bar
        //                   length of longest memory property string + null byte
        // = 236
        // This buffer is more than big enough
        char buf[256] = "{ ";

        bool first = true;

        for (int i = 0; i < 9; i++) {
            int testFlag = 1 << i;
            if ((flags & testFlag) == 0) continue;

            if (first)
                first = false;
            else
                strcat(buf, " | ");
            strcat(buf, props[i]);
        }

        strcat(buf, " }");

        return strdup(buf);
    }

    const char* toString(VkResult result) {
        switch (result) {
        case VK_SUCCESS: return "Success";
        case VK_NOT_READY: return "Not Ready";
        case VK_TIMEOUT: return "Timeout";
        case VK_EVENT_SET: return "Event Set";
        case VK_EVENT_RESET: return "Event Reset";
        case VK_INCOMPLETE: return "Incomplete";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "Out of Host Memory";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "Out of Device Memory";
        case VK_ERROR_INITIALIZATION_FAILED: return "Initialization Failed";
        case VK_ERROR_DEVICE_LOST: return "Device Lost";
        default: return "Unknown";
        }
    }

    void beginCommandBuffer(VkCommandBuffer cmdBuf, VkCommandBufferUsageFlags flags) {
        VkCommandBufferBeginInfo cbbi{};
        cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cbbi.flags = flags;

        VKCHECK(vkBeginCommandBuffer(cmdBuf, &cbbi));
    }

    void setObjectName(VkDevice device, uint64_t objectHandle, VkObjectType objectType, const char* name) {
        VkDebugUtilsObjectNameInfoEXT nameInfo{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
        nameInfo.pObjectName = name;
        nameInfo.objectHandle = objectHandle;
        nameInfo.objectType = objectType;
        nameInfo.pNext = nullptr;
        VKCHECK(vkSetDebugUtilsObjectNameEXT(device, &nameInfo));
    }

    void executeImmediately(VkDevice device, VkCommandPool commandPool, VkQueue queue, const std::function<void(VkCommandBuffer cb)>& func) {
        VkCommandBufferAllocateInfo cbai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        cbai.commandBufferCount = 1;
        cbai.commandPool = commandPool;
        cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        VkCommandBuffer cb;
        VKCHECK(vkAllocateCommandBuffers(device, &cbai, &cb));

        setObjectName(device, (uint64_t)cb, VK_OBJECT_TYPE_COMMAND_BUFFER, "Immediate Command Buffer");

        VkCommandBufferBeginInfo cbbi{};
        cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        VKCHECK(vkBeginCommandBuffer(cb, &cbbi));
        func(cb);
        VKCHECK(vkEndCommandBuffer(cb));

        VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cb;

        VKCHECK(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE));
        vkDeviceWaitIdle(device);

        vkFreeCommandBuffers(device, commandPool, 1, &cb);
    }

    BlockParams getBlockParams(VkFormat format) {
        switch (format) {
        case VK_FORMAT_R4G4_UNORM_PACK8: return BlockParams{ 1, 1, 1 };
        case VK_FORMAT_R4G4B4A4_UNORM_PACK16: return BlockParams{ 1, 1, 2 };
        case VK_FORMAT_B4G4R4A4_UNORM_PACK16: return BlockParams{ 1, 1, 2 };
        case VK_FORMAT_R5G6B5_UNORM_PACK16: return BlockParams{ 1, 1, 2 };
        case VK_FORMAT_B5G6R5_UNORM_PACK16: return BlockParams{ 1, 1, 2 };
        case VK_FORMAT_R5G5B5A1_UNORM_PACK16: return BlockParams{ 1, 1, 2 };
        case VK_FORMAT_B5G5R5A1_UNORM_PACK16: return BlockParams{ 1, 1, 2 };
        case VK_FORMAT_A1R5G5B5_UNORM_PACK16: return BlockParams{ 1, 1, 2 };
        case VK_FORMAT_R8_UNORM: return BlockParams{ 1, 1, 1 };
        case VK_FORMAT_R8_SNORM: return BlockParams{ 1, 1, 1 };
        case VK_FORMAT_R8_USCALED: return BlockParams{ 1, 1, 1 };
        case VK_FORMAT_R8_SSCALED: return BlockParams{ 1, 1, 1 };
        case VK_FORMAT_R8_UINT: return BlockParams{ 1, 1, 1 };
        case VK_FORMAT_R8_SINT: return BlockParams{ 1, 1, 1 };
        case VK_FORMAT_R8_SRGB: return BlockParams{ 1, 1, 1 };
        case VK_FORMAT_R8G8_UNORM: return BlockParams{ 1, 1, 2 };
        case VK_FORMAT_R8G8_SNORM: return BlockParams{ 1, 1, 2 };
        case VK_FORMAT_R8G8_USCALED: return BlockParams{ 1, 1, 2 };
        case VK_FORMAT_R8G8_SSCALED: return BlockParams{ 1, 1, 2 };
        case VK_FORMAT_R8G8_UINT: return BlockParams{ 1, 1, 2 };
        case VK_FORMAT_R8G8_SINT: return BlockParams{ 1, 1, 2 };
        case VK_FORMAT_R8G8_SRGB: return BlockParams{ 1, 1, 2 };
        case VK_FORMAT_R8G8B8_UNORM: return BlockParams{ 1, 1, 3 };
        case VK_FORMAT_R8G8B8_SNORM: return BlockParams{ 1, 1, 3 };
        case VK_FORMAT_R8G8B8_USCALED: return BlockParams{ 1, 1, 3 };
        case VK_FORMAT_R8G8B8_SSCALED: return BlockParams{ 1, 1, 3 };
        case VK_FORMAT_R8G8B8_UINT: return BlockParams{ 1, 1, 3 };
        case VK_FORMAT_R8G8B8_SINT: return BlockParams{ 1, 1, 3 };
        case VK_FORMAT_R8G8B8_SRGB: return BlockParams{ 1, 1, 3 };
        case VK_FORMAT_B8G8R8_UNORM: return BlockParams{ 1, 1, 3 };
        case VK_FORMAT_B8G8R8_SNORM: return BlockParams{ 1, 1, 3 };
        case VK_FORMAT_B8G8R8_USCALED: return BlockParams{ 1, 1, 3 };
        case VK_FORMAT_B8G8R8_SSCALED: return BlockParams{ 1, 1, 3 };
        case VK_FORMAT_B8G8R8_UINT: return BlockParams{ 1, 1, 3 };
        case VK_FORMAT_B8G8R8_SINT: return BlockParams{ 1, 1, 3 };
        case VK_FORMAT_B8G8R8_SRGB: return BlockParams{ 1, 1, 3 };
        case VK_FORMAT_R8G8B8A8_UNORM: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_R8G8B8A8_SNORM: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_R8G8B8A8_USCALED: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_R8G8B8A8_SSCALED: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_R8G8B8A8_UINT: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_R8G8B8A8_SINT: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_R8G8B8A8_SRGB: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_B8G8R8A8_UNORM: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_B8G8R8A8_SNORM: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_B8G8R8A8_USCALED: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_B8G8R8A8_SSCALED: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_B8G8R8A8_UINT: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_B8G8R8A8_SINT: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_B8G8R8A8_SRGB: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_A8B8G8R8_UNORM_PACK32: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_A8B8G8R8_SNORM_PACK32: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_A8B8G8R8_USCALED_PACK32: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_A8B8G8R8_SSCALED_PACK32: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_A8B8G8R8_UINT_PACK32: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_A8B8G8R8_SINT_PACK32: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_A8B8G8R8_SRGB_PACK32: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_A2R10G10B10_UNORM_PACK32: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_A2R10G10B10_SNORM_PACK32: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_A2R10G10B10_USCALED_PACK32: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_A2R10G10B10_SSCALED_PACK32: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_A2R10G10B10_UINT_PACK32: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_A2R10G10B10_SINT_PACK32: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_A2B10G10R10_SNORM_PACK32: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_A2B10G10R10_USCALED_PACK32: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_A2B10G10R10_SSCALED_PACK32: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_A2B10G10R10_UINT_PACK32: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_A2B10G10R10_SINT_PACK32: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_R16_UNORM: return BlockParams{ 1, 1, 2 };
        case VK_FORMAT_R16_SNORM: return BlockParams{ 1, 1, 2 };
        case VK_FORMAT_R16_USCALED: return BlockParams{ 1, 1, 2 };
        case VK_FORMAT_R16_SSCALED: return BlockParams{ 1, 1, 2 };
        case VK_FORMAT_R16_UINT: return BlockParams{ 1, 1, 2 };
        case VK_FORMAT_R16_SINT: return BlockParams{ 1, 1, 2 };
        case VK_FORMAT_R16_SFLOAT: return BlockParams{ 1, 1, 2 };
        case VK_FORMAT_R16G16_UNORM: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_R16G16_SNORM: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_R16G16_USCALED: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_R16G16_SSCALED: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_R16G16_UINT: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_R16G16_SINT: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_R16G16_SFLOAT: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_R16G16B16_UNORM: return BlockParams{ 1, 1, 6 };
        case VK_FORMAT_R16G16B16_SNORM: return BlockParams{ 1, 1, 6 };
        case VK_FORMAT_R16G16B16_USCALED: return BlockParams{ 1, 1, 6 };
        case VK_FORMAT_R16G16B16_SSCALED: return BlockParams{ 1, 1, 6 };
        case VK_FORMAT_R16G16B16_UINT: return BlockParams{ 1, 1, 6 };
        case VK_FORMAT_R16G16B16_SINT: return BlockParams{ 1, 1, 6 };
        case VK_FORMAT_R16G16B16_SFLOAT: return BlockParams{ 1, 1, 6 };
        case VK_FORMAT_R16G16B16A16_UNORM: return BlockParams{ 1, 1, 8 };
        case VK_FORMAT_R16G16B16A16_SNORM: return BlockParams{ 1, 1, 8 };
        case VK_FORMAT_R16G16B16A16_USCALED: return BlockParams{ 1, 1, 8 };
        case VK_FORMAT_R16G16B16A16_SSCALED: return BlockParams{ 1, 1, 8 };
        case VK_FORMAT_R16G16B16A16_UINT: return BlockParams{ 1, 1, 8 };
        case VK_FORMAT_R16G16B16A16_SINT: return BlockParams{ 1, 1, 8 };
        case VK_FORMAT_R16G16B16A16_SFLOAT: return BlockParams{ 1, 1, 8 };
        case VK_FORMAT_R32_UINT: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_R32_SINT: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_R32_SFLOAT: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_R32G32_UINT: return BlockParams{ 1, 1, 8 };
        case VK_FORMAT_R32G32_SINT: return BlockParams{ 1, 1, 8 };
        case VK_FORMAT_R32G32_SFLOAT: return BlockParams{ 1, 1, 8 };
        case VK_FORMAT_R32G32B32_UINT: return BlockParams{ 1, 1, 12 };
        case VK_FORMAT_R32G32B32_SINT: return BlockParams{ 1, 1, 12 };
        case VK_FORMAT_R32G32B32_SFLOAT: return BlockParams{ 1, 1, 12 };
        case VK_FORMAT_R32G32B32A32_UINT: return BlockParams{ 1, 1, 16 };
        case VK_FORMAT_R32G32B32A32_SINT: return BlockParams{ 1, 1, 16 };
        case VK_FORMAT_R32G32B32A32_SFLOAT: return BlockParams{ 1, 1, 16 };
        case VK_FORMAT_R64_UINT: return BlockParams{ 1, 1, 8 };
        case VK_FORMAT_R64_SINT: return BlockParams{ 1, 1, 8 };
        case VK_FORMAT_R64_SFLOAT: return BlockParams{ 1, 1, 8 };
        case VK_FORMAT_R64G64_UINT: return BlockParams{ 1, 1, 16 };
        case VK_FORMAT_R64G64_SINT: return BlockParams{ 1, 1, 16 };
        case VK_FORMAT_R64G64_SFLOAT: return BlockParams{ 1, 1, 16 };
        case VK_FORMAT_R64G64B64_UINT: return BlockParams{ 1, 1, 24 };
        case VK_FORMAT_R64G64B64_SINT: return BlockParams{ 1, 1, 24 };
        case VK_FORMAT_R64G64B64_SFLOAT: return BlockParams{ 1, 1, 24 };
        case VK_FORMAT_R64G64B64A64_UINT: return BlockParams{ 1, 1, 32 };
        case VK_FORMAT_R64G64B64A64_SINT: return BlockParams{ 1, 1, 32 };
        case VK_FORMAT_R64G64B64A64_SFLOAT: return BlockParams{ 1, 1, 32 };
        case VK_FORMAT_B10G11R11_UFLOAT_PACK32: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_D16_UNORM: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_X8_D24_UNORM_PACK32: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_D32_SFLOAT: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_S8_UINT: return BlockParams{ 1, 1, 1 };
        case VK_FORMAT_D16_UNORM_S8_UINT: return BlockParams{ 1, 1, 3 };
        case VK_FORMAT_D24_UNORM_S8_UINT: return BlockParams{ 1, 1, 4 };
        case VK_FORMAT_D32_SFLOAT_S8_UINT: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_BC1_RGB_UNORM_BLOCK: return BlockParams{ 4, 4, 8 };
        case VK_FORMAT_BC1_RGB_SRGB_BLOCK: return BlockParams{ 4, 4, 8 };
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK: return BlockParams{ 4, 4, 8 };
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK: return BlockParams{ 4, 4, 8 };
        case VK_FORMAT_BC2_UNORM_BLOCK: return BlockParams{ 4, 4, 16 };
        case VK_FORMAT_BC2_SRGB_BLOCK: return BlockParams{ 4, 4, 16 };
        case VK_FORMAT_BC3_UNORM_BLOCK: return BlockParams{ 4, 4, 16 };
        case VK_FORMAT_BC3_SRGB_BLOCK: return BlockParams{ 4, 4, 16 };
        case VK_FORMAT_BC4_UNORM_BLOCK: return BlockParams{ 4, 4, 16 };
        case VK_FORMAT_BC4_SNORM_BLOCK: return BlockParams{ 4, 4, 16 };
        case VK_FORMAT_BC5_UNORM_BLOCK: return BlockParams{ 4, 4, 16 };
        case VK_FORMAT_BC5_SNORM_BLOCK: return BlockParams{ 4, 4, 16 };
        case VK_FORMAT_BC6H_UFLOAT_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_BC6H_SFLOAT_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_BC7_UNORM_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_BC7_SRGB_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_EAC_R11_UNORM_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_EAC_R11_SNORM_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_EAC_R11G11_UNORM_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_EAC_R11G11_SNORM_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_ASTC_4x4_UNORM_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_ASTC_4x4_SRGB_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_ASTC_5x4_UNORM_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_ASTC_5x4_SRGB_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_ASTC_5x5_UNORM_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_ASTC_5x5_SRGB_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_ASTC_6x5_UNORM_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_ASTC_6x5_SRGB_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_ASTC_6x6_UNORM_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_ASTC_6x6_SRGB_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_ASTC_8x5_UNORM_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_ASTC_8x5_SRGB_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_ASTC_8x6_UNORM_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_ASTC_8x6_SRGB_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_ASTC_8x8_UNORM_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_ASTC_8x8_SRGB_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_ASTC_10x5_UNORM_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_ASTC_10x5_SRGB_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_ASTC_10x6_UNORM_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_ASTC_10x6_SRGB_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_ASTC_10x8_UNORM_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_ASTC_10x8_SRGB_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_ASTC_10x10_UNORM_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_ASTC_10x10_SRGB_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_ASTC_12x10_UNORM_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_ASTC_12x10_SRGB_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_ASTC_12x12_UNORM_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_ASTC_12x12_SRGB_BLOCK: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG: return BlockParams{ 0, 0, 0 };
        case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG: return BlockParams{ 0, 0, 0 };
        default: return BlockParams{ 0, 0, 0 };
        }
    }

    void transitionLayout(VkCommandBuffer& cb, VkImage img, VkImageLayout oldLayout, VkImageLayout newLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcMask, VkAccessFlags dstMask, VkImageAspectFlags aspectMask) {
        VkImageMemoryBarrier imageMemoryBarriers = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        imageMemoryBarriers.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarriers.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarriers.oldLayout = oldLayout;
        imageMemoryBarriers.newLayout = newLayout;
        imageMemoryBarriers.image = img;
        imageMemoryBarriers.subresourceRange = { aspectMask, 0, 1, 0, 1 };

        // Put barrier on top
        imageMemoryBarriers.srcAccessMask = srcMask;
        imageMemoryBarriers.dstAccessMask = dstMask;

        vkCmdPipelineBarrier(cb, srcStageMask, dstStageMask, VK_DEPENDENCY_BY_REGION_BIT,
            0, nullptr,
            0, nullptr,
            1, &imageMemoryBarriers);
    }

    VkSampleCountFlagBits sampleCountFlags(int sampleCount) {
        return (VkSampleCountFlagBits)sampleCount;
    }

    VkClearValue makeColorClearValue(float r, float g, float b, float a) {
        VkClearValue clearVal;
        clearVal.color.float32[0] = r;
        clearVal.color.float32[1] = g;
        clearVal.color.float32[2] = b;
        clearVal.color.float32[3] = a;
        return clearVal;
    }

    VkClearValue makeDepthStencilClearValue(float depth, uint32_t stencil) {
        VkClearValue clearVal;
        clearVal.depthStencil.depth = depth;
        clearVal.depthStencil.stencil = stencil;
        return clearVal;
    }

    ShaderModule loadShaderAsset(VkDevice device, worlds::AssetID id) {
        PHYSFS_File* file = worlds::AssetDB::openAssetFileRead(id);
        size_t size = PHYSFS_fileLength(file);
        void* buffer = std::malloc(size);

        size_t readBytes = PHYSFS_readBytes(file, buffer, size);
        assert(readBytes == size);
        PHYSFS_close(file);

        vku::ShaderModule sm{ device, static_cast<uint32_t*>(buffer), readBytes };
        std::free(buffer);
        return sm;
    }
}
