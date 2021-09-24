////////////////////////////////////////////////////////////////////////////////
//
/// Vookoo high level C++ Vulkan interface.
//
/// (C) Andy Thomason 2017 MIT License
//
/// This is a utility set alongside the vkcpp C++ interface to Vulkan which makes
/// constructing Vulkan pipelines and resources very easy for beginners.
//
/// It is expected that once familar with the Vulkan C++ interface you may wish
/// to "go it alone" but we hope that this will make the learning experience a joyful one.
//
/// You can use it with the demo framework, stand alone and mixed with C or C++ Vulkan objects.
/// It should integrate with game engines nicely.
//
////////////////////////////////////////////////////////////////////////////////
// Modified for use in WorldsEngine by Someone Somewhere

#ifndef VKU_HPP
#define VKU_HPP
#define VMA_STATIC_VULKAN_FUNCTIONS 1

#include <array>
#include <fstream>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <functional>
#include <cstddef>
#include "Libs/volk.h"
#include "vk_mem_alloc.h"
#include <assert.h>

#include <physfs.h>
#include "../../Core/Log.hpp"
#include "../../Core/Fatal.hpp"
#include "../../Core/AssetDB.hpp"
#include "vkcheck.hpp"
// just in case something pulls in windows.h
#undef min
#undef max

#define UNUSED(thing) (void)thing
namespace vku {
    /// Printf-style formatting function.
    template <class ... Args>
    std::string format(const char* fmt, Args... args) {
        int n = snprintf(nullptr, 0, fmt, args...);
        std::string result(n, '\0');
        snprintf(&*result.begin(), n + 1, fmt, args...);
        return result;
    }

    void beginCommandBuffer(VkCommandBuffer cmdBuf, VkCommandBufferUsageFlags flags = 0);

    void setObjectName(VkDevice device, uint64_t objectHandle, VkObjectType objectType, const char* name);

    /// Execute commands immediately and wait for the device to finish.
    void executeImmediately(VkDevice device, VkCommandPool commandPool, VkQueue queue, const std::function<void(VkCommandBuffer cb)>& func);

    /// Scale a value by mip level, but do not reduce to zero.
    inline uint32_t mipScale(uint32_t value, uint32_t mipLevel) {
        return std::max(value >> mipLevel, (uint32_t)1);
    }

    /// Description of blocks for compressed formats.
    struct BlockParams {
        uint8_t blockWidth;
        uint8_t blockHeight;
        uint8_t bytesPerBlock;
    };

    /// Get the details of vulkan texture formats.
    BlockParams getBlockParams(VkFormat format);

    /// Class for building shader modules and extracting metadata from shaders.
    class ShaderModule {
    public:
        ShaderModule() {
        }

        /// Construct a shader module from raw memory
        ShaderModule(VkDevice device, uint32_t* data, size_t size) {
            VkShaderModuleCreateInfo ci{};
            ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            ci.codeSize = size;
            ci.pCode = data;

            VKCHECK(vkCreateShaderModule(device, &ci, nullptr, &s.module_));

            s.ok_ = true;
        }

        /// Construct a shader module from an iterator
        template<class InIter>
        ShaderModule(VkDevice device, InIter begin, InIter end) {
            std::vector<uint32_t> opcodes;
            opcodes.assign(begin, end);
            VkShaderModuleCreateInfo ci{};
            ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            ci.codeSize = opcodes.size() * 4;
            ci.pCode = opcodes.data();

            VKCHECK(vkCreateShaderModule(device, &ci, nullptr, &s.module_));

            s.ok_ = true;
        }

        bool ok() const { return s.ok_; }
        VkShaderModule module() { return s.module_; }

    private:
        struct State {
            VkShaderModule module_;
            bool ok_ = false;
        };

        State s;
    };

    inline void transitionLayout(VkCommandBuffer& cb, VkImage img, VkImageLayout oldLayout, VkImageLayout newLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcMask, VkAccessFlags dstMask, VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT) {
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

    inline VkSampleCountFlagBits sampleCountFlags(int sampleCount) {
        return (VkSampleCountFlagBits)sampleCount;
    }

    inline VkClearValue makeColorClearValue(float r, float g, float b, float a) {
        VkClearValue clearVal;
        clearVal.color.float32[0] = r;
        clearVal.color.float32[1] = g;
        clearVal.color.float32[2] = b;
        clearVal.color.float32[3] = a;
        return clearVal;
    }

    inline VkClearValue makeDepthStencilClearValue(float depth, uint32_t stencil) {
        VkClearValue clearVal;
        clearVal.depthStencil.depth = depth;
        clearVal.depthStencil.stencil = stencil;
        return clearVal;
    }

    inline ShaderModule loadShaderAsset(VkDevice device, worlds::AssetID id) {
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
} // namespace vku

#undef UNUSED
#endif // VKU_HPP
