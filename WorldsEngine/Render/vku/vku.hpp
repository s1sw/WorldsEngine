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
// just in case something pulls in windows.h
#undef min
#undef max

#define UNUSED(thing) (void)thing
#define VKCHECK(expr) vku::checkVkResult(expr, __FILE__, __LINE__)
namespace vku {
    /// Printf-style formatting function.
    template <class ... Args>
    std::string format(const char* fmt, Args... args) {
        int n = snprintf(nullptr, 0, fmt, args...);
        std::string result(n, '\0');
        snprintf(&*result.begin(), n + 1, fmt, args...);
        return result;
    }

    inline const char* toString(VkPhysicalDeviceType type) {
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

    inline const char* toString(VkMemoryPropertyFlags flags) {
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

    inline const char* toString(VkResult result) {
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

    inline VkResult checkVkResult(VkResult result, const char* file, int line) {
        switch (result) {
        case VK_ERROR_OUT_OF_HOST_MEMORY:
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        case VK_ERROR_DEVICE_LOST:
            fatalErrInternal(toString(result), file, line);
            break;
        default: return result;
        }

        return result;
    }

    // why.
    struct ImageLayout {
    public:
        enum Bits {
            Undefined = VK_IMAGE_LAYOUT_UNDEFINED,
            General = VK_IMAGE_LAYOUT_GENERAL,
            ColorAttachmentOptimal = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            DepthStencilAttachmentOptimal = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            DepthStencilReadOnlyOptimal = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
            ShaderReadOnlyOptimal = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            TransferSrcOptimal = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            TransferDstOptimal = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            Preinitialized = VK_IMAGE_LAYOUT_PREINITIALIZED,
            DepthReadOnlyStencilAttachmentOptimal = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL,
            DepthAttachmentStencilReadOnlyOptimal = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL,
            DepthAttachmentOptimal = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            DepthReadOnlyOptimal = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
            StencilAttachmentOptimal = VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL,
            StencilReadOnlyOptimal = VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL,
            PresentSrcKHR = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            SharedPresentKHR = VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR,
            ShadingRateOptimalNV = VK_IMAGE_LAYOUT_SHADING_RATE_OPTIMAL_NV,
            FragmentDensityMapOptimalEXT = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT
        };

        Bits value;

        ImageLayout(Bits val) : value(val) {}
        ImageLayout(VkImageLayout val) : value((Bits)val) {}

        operator Bits() { return value; }
        operator VkImageLayout() { return (VkImageLayout)value; }

        bool operator==(ImageLayout other) { return value == other.value; }
    };

    struct AccessFlags {
    public:
        enum Bits {
            IndirectCommandRead = VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
            IndexRead = VK_ACCESS_INDEX_READ_BIT,
            AccessVertexAttrubuteRead = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
            UniformRead = VK_ACCESS_UNIFORM_READ_BIT,
            InputAttachmentRead = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
            ShaderRead = VK_ACCESS_SHADER_READ_BIT,
            ShaderWrite = VK_ACCESS_SHADER_WRITE_BIT,
            ColorAttachmentRead = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
            ColorAttachmentWrite = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            DepthStencilAttachmentRead = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
            DepthStencilAttachmentWrite = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            TransferRead = VK_ACCESS_TRANSFER_READ_BIT,
            TransferWrite = VK_ACCESS_TRANSFER_WRITE_BIT,
            HostRead = VK_ACCESS_HOST_READ_BIT,
            HostWrite = VK_ACCESS_HOST_WRITE_BIT,
            MemoryRead = VK_ACCESS_MEMORY_READ_BIT,
            MemoryWrite = VK_ACCESS_MEMORY_WRITE_BIT,
            ConditionalRenderinReadEXT = VK_ACCESS_CONDITIONAL_RENDERING_READ_BIT_EXT,
            ColorAttachmentReadNoncoherentEXT = VK_ACCESS_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT,
            AccelerationStructureReadKHR = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
            AccelerationStructureWriteKHR = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
            ShadingReadImageReadNV = VK_ACCESS_SHADING_RATE_IMAGE_READ_BIT_NV,
            FragmentDensityMapReadEXT = VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT,
            CommandPreprocessReadNV = VK_ACCESS_COMMAND_PREPROCESS_READ_BIT_NV,
            CommandPreprocessWriteNV = VK_ACCESS_COMMAND_PREPROCESS_WRITE_BIT_NV,
            FragmentShadingRateAttachmentReadKHR = VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR,
        };

        Bits value;

        AccessFlags(Bits val) : value(val) {}
        AccessFlags(VkAccessFlags val) : value((Bits)val) {}

        operator Bits() { return value; }
        operator VkAccessFlags() { return (VkAccessFlags)value; }
    };

    struct PipelineStageFlags {
    public:
        enum Bits {
            TopOfPipe = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            DrawIndirect = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
            VertexInput = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
            VertexShader = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
            TessellationControlShader = VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT,
            TessellationEvaluationShader = VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT,
            GeometryShader = VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT,
            FragmentShader = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            EarlyFragmentTests = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            LateFragmentTests = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            ColorAttachmentOutput = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            ComputeShader = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            Transfer = VK_PIPELINE_STAGE_TRANSFER_BIT,
            BottomOfPipe = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            Host = VK_PIPELINE_STAGE_HOST_BIT,
            AllGraphics = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
            AllCommands = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            TransformFeedbackEXT = VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT,
            ConditionalRenderingEXT = VK_PIPELINE_STAGE_CONDITIONAL_RENDERING_BIT_EXT,
            AcclerationStructureBuildKHR = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            RayTracingShaderKHR = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            ShadingRateImageNV = VK_PIPELINE_STAGE_SHADING_RATE_IMAGE_BIT_NV,
            TaskShaderNV = VK_PIPELINE_STAGE_TASK_SHADER_BIT_NV,
            MeshShaderNV = VK_PIPELINE_STAGE_MESH_SHADER_BIT_NV,
            FragmentDensityProcessEXT = VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT,
            ComandPreprocessNV = VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_NV
        };

        Bits value;

        PipelineStageFlags(Bits val) : value(val) {}
        PipelineStageFlags(VkPipelineStageFlags val) : value((Bits)val) {}

        operator Bits() { return value; }
        operator VkPipelineStageFlags() { return (VkPipelineStageFlags)value; }

        PipelineStageFlags operator|(PipelineStageFlags other) {
            return PipelineStageFlags(value | other.value);
        }
    };

    struct ImageUsageFlags {
    public:
        enum Bits {
            TransferSrc = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            TransferDst = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            Sampled = VK_IMAGE_USAGE_SAMPLED_BIT,
            Storage = VK_IMAGE_USAGE_STORAGE_BIT,
            ColorAttachment = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            DepthStencilAttachment = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            TransientAttachment = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
            InputAttachment = VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
            ShadingRateAttachmentKHR = VK_IMAGE_USAGE_SHADING_RATE_IMAGE_BIT_NV,
            FragmentDensityMap = VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT
        };

        Bits value;

        ImageUsageFlags(Bits val) : value(val) {}
        ImageUsageFlags(VkImageUsageFlags val) : value((Bits)val) {}

        operator Bits() { return value; }
        operator VkImageUsageFlags() { return (VkImageUsageFlags)value; }

        ImageUsageFlags operator|(ImageUsageFlags other) {
            return ImageUsageFlags(value | other.value);
        }

        ImageUsageFlags operator&(const ImageUsageFlags& other) {
            return ImageUsageFlags(value & other.value);
        }
    };

    inline void beginCommandBuffer(VkCommandBuffer cmdBuf, VkCommandBufferUsageFlags flags = 0) {
        VkCommandBufferBeginInfo cbbi{};
        cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cbbi.flags = flags;

        VKCHECK(vkBeginCommandBuffer(cmdBuf, &cbbi));
    }

    /// Utility function for finding memory types for uniforms and images.
    inline int findMemoryTypeIndex(const VkPhysicalDeviceMemoryProperties& memprops, uint32_t memoryTypeBits, VkMemoryPropertyFlags search) {
        for (uint32_t i = 0; i != memprops.memoryTypeCount; ++i, memoryTypeBits >>= 1) {
            if (memoryTypeBits & 1) {
                if ((memprops.memoryTypes[i].propertyFlags & search) == search) {
                    return i;
                }
            }
        }
        return -1;
    }

    inline void setObjectName(VkDevice device, uint64_t objectHandle, VkObjectType objectType, const char* name) {
        VkDebugUtilsObjectNameInfoEXT nameInfo;
        nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        nameInfo.pObjectName = name;
        nameInfo.objectHandle = objectHandle;
        nameInfo.objectType = objectType;
        nameInfo.pNext = nullptr;
        VKCHECK(vkSetDebugUtilsObjectNameEXT(device, &nameInfo));
    }

    /// Execute commands immediately and wait for the device to finish.
    inline void executeImmediately(VkDevice device, VkCommandPool commandPool, VkQueue queue, const std::function<void(VkCommandBuffer cb)>& func) {
        VkCommandBufferAllocateInfo cbai{}; // { commandPool, VkCommandBufferLevel::ePrimary, 1 };
        cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cbai.commandBufferCount = 1;
        cbai.commandPool = commandPool;
        cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        VkCommandBuffer cb;
        VKCHECK(vkAllocateCommandBuffers(device, &cbai, &cb));

        VkDebugUtilsObjectNameInfoEXT nameInfo;
        nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        nameInfo.pObjectName = "Immediate Command Buffer";
        nameInfo.objectHandle = (uint64_t)cb;
        nameInfo.objectType = VK_OBJECT_TYPE_COMMAND_BUFFER;
        nameInfo.pNext = nullptr;
        auto setObjName = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT");
        setObjName(device, &nameInfo);

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

    /// Scale a value by mip level, but do not reduce to zero.
    inline uint32_t mipScale(uint32_t value, uint32_t mipLevel) {
        return std::max(value >> mipLevel, (uint32_t)1);
    }

    /// Load a binary file into a vector.
    /// The vector will be zero-length if this fails.
    inline std::vector<uint8_t> loadFile(const std::string& filename) {
        std::ifstream is(filename, std::ios::binary | std::ios::ate);
        std::vector<uint8_t> bytes;
        if (!is.fail()) {
            size_t size = is.tellg();
            is.seekg(0);
            bytes.resize(size);
            is.read((char*)bytes.data(), size);
        }
        return bytes;
    }

    /// Description of blocks for compressed formats.
    struct BlockParams {
        uint8_t blockWidth;
        uint8_t blockHeight;
        uint8_t bytesPerBlock;
    };

    /// Get the details of vulkan texture formats.
    inline BlockParams getBlockParams(VkFormat format) {
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

    /// Factory for instances.
    class InstanceMaker {
    public:
        InstanceMaker() {
        }

        /// Set the default layers and extensions.
        InstanceMaker& defaultLayers() {
            layers_.push_back("VK_LAYER_LUNARG_standard_validation");
            instance_extensions_.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#ifdef VKU_SURFACE
            instance_extensions_.push_back(VKU_SURFACE);
#endif
            instance_extensions_.push_back("VK_KHR_surface");
#if defined( __APPLE__ ) && defined(VK_EXT_METAL_SURFACE_EXTENSION_NAME)
            instance_extensions_.push_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
#endif //__APPLE__
            return *this;
        }

        /// Add a layer. eg. "VK_LAYER_LUNARG_standard_validation"
        InstanceMaker& layer(const char* layer) {
            layers_.push_back(layer);
            return *this;
        }

        /// Add an extension. eg. VK_EXT_DEBUG_REPORT_EXTENSION_NAME
        InstanceMaker& extension(const char* layer) {
            instance_extensions_.push_back(layer);
            return *this;
        }

        /// Set the name of the application.
        InstanceMaker& applicationName(const char* pApplicationName_) {
            app_info_.pApplicationName = pApplicationName_;
            return *this;
        }

        /// Set the version of the application.
        InstanceMaker& applicationVersion(uint32_t applicationVersion_) {
            app_info_.applicationVersion = applicationVersion_;
            return *this;
        }

        /// Set the name of the engine.
        InstanceMaker& engineName(const char* pEngineName_) {
            app_info_.pEngineName = pEngineName_;
            return *this;
        }

        /// Set the version of the engine.
        InstanceMaker& engineVersion(uint32_t engineVersion_) {
            app_info_.engineVersion = engineVersion_;
            return *this;
        }

        /// Set the version of the api.
        InstanceMaker& apiVersion(uint32_t apiVersion_) {
            app_info_.apiVersion = apiVersion_;
            return *this;
        }

        /// Create an instance.
        VkInstance create() {
            VkInstanceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            createInfo.pApplicationInfo = &app_info_;
            createInfo.enabledLayerCount = layers_.size();
            createInfo.enabledExtensionCount = instance_extensions_.size();

            createInfo.ppEnabledLayerNames = layers_.data();
            createInfo.ppEnabledExtensionNames = instance_extensions_.data();

            VkInstance instance;
            vkCreateInstance(&createInfo, nullptr, &instance);

            return instance;
        }
    private:
        std::vector<const char*> layers_;
        std::vector<const char*> instance_extensions_;
        VkApplicationInfo app_info_{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
    };

    /// Factory for devices.
    class DeviceMaker {
    public:
        /// Make queues and a logical device for a certain physical device.
        DeviceMaker() : pNext(nullptr) {
        }

        /// Set the default layers and extensions.
        DeviceMaker& defaultLayers() {
            layers_.push_back("VK_LAYER_LUNARG_standard_validation");
            device_extensions_.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
            return *this;
        }

        /// Add a layer. eg. "VK_LAYER_LUNARG_standard_validation"
        DeviceMaker& layer(const char* layer) {
            layers_.push_back(layer);
            return *this;
        }

        /// Add an extension. eg. VK_EXT_DEBUG_REPORT_EXTENSION_NAME
        DeviceMaker& extension(const char* layer) {
            device_extensions_.push_back(layer);
            return *this;
        }

        /// Add one or more queues to the device from a certain family.
        DeviceMaker& queue(uint32_t familyIndex, float priority = 0.0f, uint32_t n = 1) {
            queue_priorities_.emplace_back(n, priority);

            qci_.emplace_back(
                VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                nullptr,
                VkDeviceQueueCreateFlags{},
                familyIndex, n,
                queue_priorities_.back().data()
            );

            return *this;
        }

        DeviceMaker& setPNext(void* next) {
            pNext = next;
            return *this;
        }

        DeviceMaker& setFeatures(VkPhysicalDeviceFeatures& features) {
            createFeatures = features;
            return *this;
        }

        /// Create a new logical device.
        VkDevice create(VkPhysicalDevice physical_device) {
            VkDeviceCreateInfo dci{
                VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, nullptr,
                {}, (uint32_t)qci_.size(), qci_.data(),
                (uint32_t)layers_.size(), layers_.data(),
                (uint32_t)device_extensions_.size(), device_extensions_.data(), &createFeatures };
            dci.pNext = pNext;

            VkDevice device;
            VKCHECK(vkCreateDevice(physical_device, &dci, nullptr, &device));

            return device;
        }
    private:
        std::vector<const char*> layers_;
        std::vector<const char*> device_extensions_;
        std::vector<std::vector<float> > queue_priorities_;
        std::vector<VkDeviceQueueCreateInfo> qci_;
        VkPhysicalDeviceFeatures createFeatures;
        void* pNext;
    };

    class DebugCallback {
    public:
        DebugCallback() {
        }

        DebugCallback(
            VkInstance instance,
            VkDebugReportFlagsEXT flags =
            VK_DEBUG_REPORT_WARNING_BIT_EXT |
            VK_DEBUG_REPORT_ERROR_BIT_EXT
        ) : instance_(instance) {
            auto ci = VkDebugReportCallbackCreateInfoEXT{
                VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
                nullptr, flags, &debugCallback, nullptr };

            VkDebugReportCallbackEXT cb{};
            VKCHECK(vkCreateDebugReportCallbackEXT(
                instance_, &ci,
                nullptr, &cb
            ));
            callback_ = cb;
        }

        void reset() {
            if (callback_) {
                auto vkDestroyDebugReportCallbackEXT =
                    (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance_,
                        "vkDestroyDebugReportCallbackEXT");
                vkDestroyDebugReportCallbackEXT(instance_, callback_, nullptr);
                callback_ = VK_NULL_HANDLE;
            }
        }
    private:
        // Report any errors or warnings.
        static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
            VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType,
            uint64_t object, size_t location, int32_t messageCode,
            const char* pLayerPrefix, const char* pMessage, void* pUserData) {
            UNUSED(objectType);
            UNUSED(object);
            UNUSED(location);
            UNUSED(messageCode);
            UNUSED(pLayerPrefix);
            UNUSED(pMessage);
            UNUSED(pUserData);

            if ((flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) == VK_DEBUG_REPORT_ERROR_BIT_EXT) {
                logErr(worlds::WELogCategoryRender, "Vulkan: %s\n", pMessage);
            } else if ((flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) == VK_DEBUG_REPORT_WARNING_BIT_EXT) {
                logWarn(worlds::WELogCategoryRender, "Vulkan: %s\n", pMessage);
            } else {
                logMsg(worlds::WELogCategoryRender, "Vulkan: %s\n", pMessage);
            }
            return VK_FALSE;
        }
        VkDebugReportCallbackEXT callback_ = VK_NULL_HANDLE;
        VkInstance instance_ = VK_NULL_HANDLE;
    };

    /// Factory for renderpasses.
    /// example:
    ///     RenderpassMaker rpm;
    ///     rpm.subpassBegin(VK_PIPELINE_BIND_POINT_GRAPHICS);
    ///     rpm.subpassColorAttachment(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    ///
    ///     rpm.attachmentDescription(attachmentDesc);
    ///     rpm.subpassDependency(dependency);
    ///     s.renderPass_ = rpm.create(device);
    class RenderpassMaker {
    public:
        RenderpassMaker() {
        }

        /// Begin an attachment description.
        /// After this you can call attachment* many times
        void attachmentBegin(VkFormat format) {
            VkAttachmentDescription desc{
                .flags = {},
                .format = format,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_UNDEFINED
            };
            s.attachmentDescriptions.push_back(desc);
        }

        void attachmentFlags(VkAttachmentDescriptionFlags value) { s.attachmentDescriptions.back().flags = value; };
        void attachmentFormat(VkFormat value) { s.attachmentDescriptions.back().format = value; };
        void attachmentSamples(VkSampleCountFlagBits value) { s.attachmentDescriptions.back().samples = value; };
        void attachmentLoadOp(VkAttachmentLoadOp value) { s.attachmentDescriptions.back().loadOp = value; };
        void attachmentStoreOp(VkAttachmentStoreOp value) { s.attachmentDescriptions.back().storeOp = value; };
        void attachmentStencilLoadOp(VkAttachmentLoadOp value) { s.attachmentDescriptions.back().stencilLoadOp = value; };
        void attachmentStencilStoreOp(VkAttachmentStoreOp value) { s.attachmentDescriptions.back().stencilStoreOp = value; };
        void attachmentInitialLayout(VkImageLayout value) { s.attachmentDescriptions.back().initialLayout = value; };
        void attachmentFinalLayout(VkImageLayout value) { s.attachmentDescriptions.back().finalLayout = value; };

        /// Start a subpass description.
        /// After this you can can call subpassColorAttachment many times
        /// and subpassDepthStencilAttachment once.
        void subpassBegin(VkPipelineBindPoint bp) {
            VkSubpassDescription desc{};
            desc.pipelineBindPoint = bp;
            s.subpassDescriptions.push_back(desc);
        }

        void subpassColorAttachment(VkImageLayout layout, uint32_t attachment) {
            VkSubpassDescription& subpass = s.subpassDescriptions.back();
            auto* p = getAttachmentReference();
            p->layout = layout;
            p->attachment = attachment;
            if (subpass.colorAttachmentCount == 0) {
                subpass.pColorAttachments = p;
            }
            subpass.colorAttachmentCount++;
        }

        void subpassDepthStencilAttachment(VkImageLayout layout, uint32_t attachment) {
            VkSubpassDescription& subpass = s.subpassDescriptions.back();
            auto* p = getAttachmentReference();
            p->layout = layout;
            p->attachment = attachment;
            subpass.pDepthStencilAttachment = p;
        }

        VkRenderPass create(VkDevice device) const {
            VkRenderPassCreateInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            renderPassInfo.attachmentCount = (uint32_t)s.attachmentDescriptions.size();
            renderPassInfo.pAttachments = s.attachmentDescriptions.data();
            renderPassInfo.subpassCount = (uint32_t)s.subpassDescriptions.size();
            renderPassInfo.pSubpasses = s.subpassDescriptions.data();
            renderPassInfo.dependencyCount = (uint32_t)s.subpassDependencies.size();
            renderPassInfo.pDependencies = s.subpassDependencies.data();
            renderPassInfo.pNext = s.pNext;

            VkRenderPass pass;
            VKCHECK(vkCreateRenderPass(device, &renderPassInfo, nullptr, &pass));

            return pass;
        }

        void dependencyBegin(uint32_t srcSubpass, uint32_t dstSubpass) {
            VkSubpassDependency desc{};
            desc.srcSubpass = srcSubpass;
            desc.dstSubpass = dstSubpass;
            s.subpassDependencies.push_back(desc);
        }

        void dependencySrcSubpass(uint32_t value) { s.subpassDependencies.back().srcSubpass = value; };
        void dependencyDstSubpass(uint32_t value) { s.subpassDependencies.back().dstSubpass = value; };
        void dependencySrcStageMask(VkPipelineStageFlags value) { s.subpassDependencies.back().srcStageMask = value; };
        void dependencyDstStageMask(VkPipelineStageFlags value) { s.subpassDependencies.back().dstStageMask = value; };
        void dependencySrcAccessMask(VkAccessFlags value) { s.subpassDependencies.back().srcAccessMask = value; };
        void dependencyDstAccessMask(VkAccessFlags value) { s.subpassDependencies.back().dstAccessMask = value; };
        void dependencyDependencyFlags(VkDependencyFlags value) { s.subpassDependencies.back().dependencyFlags = value; };
        void setPNext(void* pn) { s.pNext = pn; }
    private:
        constexpr static int max_refs = 64;

        VkAttachmentReference* getAttachmentReference() {
            return (s.num_refs < max_refs) ? &s.attachmentReferences[s.num_refs++] : nullptr;
        }

        struct State {
            std::vector<VkAttachmentDescription> attachmentDescriptions;
            std::vector<VkSubpassDescription> subpassDescriptions;
            std::vector<VkSubpassDependency> subpassDependencies;
            std::array<VkAttachmentReference, max_refs> attachmentReferences;
            void* pNext = nullptr;
            int num_refs = 0;
            bool ok_ = false;
        };

        State s;
    };

    /// Class for building shader modules and extracting metadata from shaders.
    class ShaderModule {
    public:
        ShaderModule() {
        }

        /// Construct a shader module from a file
        ShaderModule(VkDevice device, const std::string& filename) {
            auto file = std::ifstream(filename, std::ios::binary);
            if (file.bad()) {
                return;
            }

            file.seekg(0, std::ios::end);
            int length = (int)file.tellg();

            char* buf = (char*)std::malloc(length);

            file.seekg(0, std::ios::beg);
            file.read(buf, length);

            VkShaderModuleCreateInfo ci{};
            ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            ci.codeSize = length;
            ci.pCode = (uint32_t*)buf;

            VKCHECK(vkCreateShaderModule(device, &ci, nullptr, &s.module_));

            std::free(buf);

            s.ok_ = true;
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

    /// A class for building pipeline layouts.
    /// Pipeline layouts describe the descriptor sets and push constants used by the shaders.
    class PipelineLayoutMaker {
    public:
        PipelineLayoutMaker() {}

        /// Create a pipeline layout object.
        VkPipelineLayout create(VkDevice device) const {
            VkPipelineLayoutCreateInfo pipelineLayoutInfo{
                VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                nullptr, {}, (uint32_t)setLayouts_.size(),
                setLayouts_.data(), (uint32_t)pushConstantRanges_.size(),
                pushConstantRanges_.data() };

            VkPipelineLayout layout;
            VKCHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &layout));

            return layout;
        }

        /// Add a descriptor set layout to the pipeline.
        void descriptorSetLayout(VkDescriptorSetLayout layout) {
            setLayouts_.push_back(layout);
        }

        /// Add a push constant range to the pipeline.
        /// These describe the size and location of variables in the push constant area.
        void pushConstantRange(VkShaderStageFlags stageFlags_, uint32_t offset_, uint32_t size_) {
            pushConstantRanges_.emplace_back(stageFlags_, offset_, size_);
        }

    private:
        std::vector<VkDescriptorSetLayout> setLayouts_;
        std::vector<VkPushConstantRange> pushConstantRanges_;
    };

    /// A class for building pipelines.
    /// All the state of the pipeline is exposed through individual calls.
    /// The pipeline encapsulates all the OpenGL state in a single object.
    /// This includes vertex buffer layouts, blend operations, shaders, line width etc.
    /// This class exposes all the values as individuals so a pipeline can be customised.
    /// The default is to generate a working pipeline.
    class PipelineMaker {
    public:
        PipelineMaker(uint32_t width, uint32_t height) {
            inputAssemblyState_.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            viewport_ = VkViewport{ 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
            scissor_ = VkRect2D{ {0, 0}, {width, height} };
            rasterizationState_.lineWidth = 1.0f;

            multisampleState_.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

            // Set up depth test, but do not enable it.
            depthStencilState_.depthTestEnable = VK_FALSE;
            depthStencilState_.depthWriteEnable = VK_TRUE;
            depthStencilState_.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
            depthStencilState_.depthBoundsTestEnable = VK_FALSE;
            depthStencilState_.back.failOp = VK_STENCIL_OP_KEEP;
            depthStencilState_.back.passOp = VK_STENCIL_OP_KEEP;
            depthStencilState_.back.compareOp = VK_COMPARE_OP_ALWAYS;
            depthStencilState_.stencilTestEnable = VK_FALSE;
            depthStencilState_.front = depthStencilState_.back;
        }

        VkPipeline create(VkDevice device,
            const VkPipelineCache& pipelineCache,
            const VkPipelineLayout& pipelineLayout,
            const VkRenderPass& renderPass, bool defaultBlend = true) {

            // Add default colour blend attachment if necessary.
            if (colorBlendAttachments_.empty() && defaultBlend) {
                VkPipelineColorBlendAttachmentState blend{};
                blend.blendEnable = 0;
                blend.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
                blend.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
                blend.colorBlendOp = VK_BLEND_OP_ADD;
                blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
                blend.alphaBlendOp = VK_BLEND_OP_ADD;
                blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
                colorBlendAttachments_.push_back(blend);
            }

            auto count = (uint32_t)colorBlendAttachments_.size();
            colorBlendState_.attachmentCount = count;
            colorBlendState_.pAttachments = count ? colorBlendAttachments_.data() : nullptr;

            VkPipelineViewportStateCreateInfo viewportState{
                VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr,
                {}, 1, &viewport_, 1, &scissor_ };

            VkPipelineVertexInputStateCreateInfo vertexInputState{};
            vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertexInputState.vertexAttributeDescriptionCount = (uint32_t)vertexAttributeDescriptions_.size();
            vertexInputState.pVertexAttributeDescriptions = vertexAttributeDescriptions_.data();
            vertexInputState.vertexBindingDescriptionCount = (uint32_t)vertexBindingDescriptions_.size();
            vertexInputState.pVertexBindingDescriptions = vertexBindingDescriptions_.data();

            VkPipelineDynamicStateCreateInfo dynState{
                VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr,
                {}, (uint32_t)dynamicState_.size(), dynamicState_.data() };

            VkGraphicsPipelineCreateInfo pipelineInfo{};
            pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipelineInfo.pVertexInputState = &vertexInputState;
            pipelineInfo.stageCount = (uint32_t)modules_.size();
            pipelineInfo.pStages = modules_.data();
            pipelineInfo.pInputAssemblyState = &inputAssemblyState_;
            pipelineInfo.pViewportState = &viewportState;
            pipelineInfo.pRasterizationState = &rasterizationState_;
            pipelineInfo.pMultisampleState = &multisampleState_;
            pipelineInfo.pColorBlendState = &colorBlendState_;
            pipelineInfo.pDepthStencilState = &depthStencilState_;
            pipelineInfo.layout = pipelineLayout;
            pipelineInfo.renderPass = renderPass;
            pipelineInfo.pDynamicState = dynamicState_.empty() ? nullptr : &dynState;
            pipelineInfo.subpass = subpass_;

            VkPipeline pipeline;
            VKCHECK(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &pipeline));
            return pipeline;
        }

        /// Add a shader module to the pipeline.
        void shader(VkShaderStageFlagBits stage, vku::ShaderModule& shader,
            const char* entryPoint = "main", VkSpecializationInfo* pSpecializationInfo = nullptr) {
            VkPipelineShaderStageCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            info.module = shader.module();
            info.pName = entryPoint;
            info.stage = stage;
            info.pSpecializationInfo = pSpecializationInfo;
            modules_.emplace_back(info);
        }

        /// Add a shader module to the pipeline.
        void shader(VkShaderStageFlagBits stage, VkShaderModule shader,
            const char* entryPoint = "main", VkSpecializationInfo* pSpecializationInfo = nullptr) {
            VkPipelineShaderStageCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            info.module = shader;
            info.pName = entryPoint;
            info.stage = stage;
            info.pSpecializationInfo = pSpecializationInfo;
            modules_.emplace_back(info);
        }

        /// Add a blend state to the pipeline for one colour attachment.
        /// If you don't do this, a default is used.
        void colorBlend(const VkPipelineColorBlendAttachmentState& state) {
            colorBlendAttachments_.push_back(state);
        }

        void subPass(uint32_t subpass) {
            subpass_ = subpass;
        }

        /// Begin setting colour blend value
        /// If you don't do this, a default is used.
        /// Follow this with blendEnable() blendSrcColorBlendFactor() etc.
        /// Default is a regular alpha blend.
        void blendBegin(VkBool32 enable) {
            colorBlendAttachments_.emplace_back();
            auto& blend = colorBlendAttachments_.back();
            blend.blendEnable = enable;
            blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blend.colorBlendOp = VK_BLEND_OP_ADD;
            blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blend.alphaBlendOp = VK_BLEND_OP_ADD;
            blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        }

        /// Enable or disable blending (called after blendBegin())
        void blendEnable(VkBool32 value) { colorBlendAttachments_.back().blendEnable = value; }

        /// Source colour blend factor (called after blendBegin())
        void blendSrcColorBlendFactor(VkBlendFactor value) { colorBlendAttachments_.back().srcColorBlendFactor = value; }

        /// Destination colour blend factor (called after blendBegin())
        void blendDstColorBlendFactor(VkBlendFactor value) { colorBlendAttachments_.back().dstColorBlendFactor = value; }

        /// Blend operation (called after blendBegin())
        void blendColorBlendOp(VkBlendOp value) { colorBlendAttachments_.back().colorBlendOp = value; }

        /// Source alpha blend factor (called after blendBegin())
        void blendSrcAlphaBlendFactor(VkBlendFactor value) { colorBlendAttachments_.back().srcAlphaBlendFactor = value; }

        /// Destination alpha blend factor (called after blendBegin())
        void blendDstAlphaBlendFactor(VkBlendFactor value) { colorBlendAttachments_.back().dstAlphaBlendFactor = value; }

        /// Alpha operation (called after blendBegin())
        void blendAlphaBlendOp(VkBlendOp value) { colorBlendAttachments_.back().alphaBlendOp = value; }

        /// Colour write mask (called after blendBegin())
        void blendColorWriteMask(VkColorComponentFlags value) { colorBlendAttachments_.back().colorWriteMask = value; }

        /// Add a vertex attribute to the pipeline.
        void vertexAttribute(uint32_t location_, uint32_t binding_, VkFormat format_, uint32_t offset_) {
            vertexAttributeDescriptions_.push_back({ location_, binding_, format_, offset_ });
        }

        /// Add a vertex attribute to the pipeline.
        void vertexAttribute(const VkVertexInputAttributeDescription& desc) {
            vertexAttributeDescriptions_.push_back(desc);
        }

        /// Add a vertex binding to the pipeline.
        /// Usually only one of these is needed to specify the stride.
        /// Vertices can also be delivered one per instance.
        void vertexBinding(uint32_t binding_, uint32_t stride_, VkVertexInputRate inputRate_ = VK_VERTEX_INPUT_RATE_VERTEX) {
            vertexBindingDescriptions_.push_back({ binding_, stride_, inputRate_ });
        }

        /// Add a vertex binding to the pipeline.
        /// Usually only one of these is needed to specify the stride.
        /// Vertices can also be delivered one per instance.
        void vertexBinding(const VkVertexInputBindingDescription& desc) {
            vertexBindingDescriptions_.push_back(desc);
        }

        /// Specify the topology of the pipeline.
        /// Usually this is a triangle list, but points and lines are possible too.
        PipelineMaker& topology(VkPrimitiveTopology topology) { inputAssemblyState_.topology = topology; return *this; }

        /// Enable or disable primitive restart.
        /// If using triangle strips, for example, this allows a special index value (0xffff or 0xffffffff) to start a new strip.
        PipelineMaker& primitiveRestartEnable(VkBool32 primitiveRestartEnable) { inputAssemblyState_.primitiveRestartEnable = primitiveRestartEnable; return *this; }

        /// Set a whole new input assembly state.
        /// Note you can set individual values with their own call
        PipelineMaker& inputAssemblyState(const VkPipelineInputAssemblyStateCreateInfo& value) { inputAssemblyState_ = value; return *this; }

        /// Set the viewport value.
        /// Usually there is only one viewport, but you can have multiple viewports active for rendering cubemaps or VR stereo pair
        PipelineMaker& viewport(const VkViewport& value) { viewport_ = value; return *this; }

        /// Set the scissor value.
        /// This defines the area that the fragment shaders can write to. For example, if you are rendering a portal or a mirror.
        PipelineMaker& scissor(const VkRect2D& value) { scissor_ = value; return *this; }

        /// Set a whole rasterization state.
        /// Note you can set individual values with their own call
        PipelineMaker& rasterizationState(const VkPipelineRasterizationStateCreateInfo& value) { rasterizationState_ = value; return *this; }
        PipelineMaker& depthClampEnable(VkBool32 value) { rasterizationState_.depthClampEnable = value; return *this; }
        PipelineMaker& rasterizerDiscardEnable(VkBool32 value) { rasterizationState_.rasterizerDiscardEnable = value; return *this; }
        PipelineMaker& polygonMode(VkPolygonMode value) { rasterizationState_.polygonMode = value; return *this; }
        PipelineMaker& cullMode(VkCullModeFlags value) { rasterizationState_.cullMode = value; return *this; }
        PipelineMaker& frontFace(VkFrontFace value) { rasterizationState_.frontFace = value; return *this; }
        PipelineMaker& depthBiasEnable(VkBool32 value) { rasterizationState_.depthBiasEnable = value; return *this; }
        PipelineMaker& depthBiasConstantFactor(float value) { rasterizationState_.depthBiasConstantFactor = value; return *this; }
        PipelineMaker& depthBiasClamp(float value) { rasterizationState_.depthBiasClamp = value; return *this; }
        PipelineMaker& depthBiasSlopeFactor(float value) { rasterizationState_.depthBiasSlopeFactor = value; return *this; }
        PipelineMaker& lineWidth(float value) { rasterizationState_.lineWidth = value; return *this; }


        /// Set a whole multi sample state.
        /// Note you can set individual values with their own call
        PipelineMaker& multisampleState(const VkPipelineMultisampleStateCreateInfo& value) { multisampleState_ = value; return *this; }
        PipelineMaker& rasterizationSamples(VkSampleCountFlagBits value) { multisampleState_.rasterizationSamples = value; return *this; }
        PipelineMaker& sampleShadingEnable(VkBool32 value) { multisampleState_.sampleShadingEnable = value; return *this; }
        PipelineMaker& minSampleShading(float value) { multisampleState_.minSampleShading = value; return *this; }
        PipelineMaker& pSampleMask(const VkSampleMask* value) { multisampleState_.pSampleMask = value; return *this; }
        PipelineMaker& alphaToCoverageEnable(VkBool32 value) { multisampleState_.alphaToCoverageEnable = value; return *this; }
        PipelineMaker& alphaToOneEnable(VkBool32 value) { multisampleState_.alphaToOneEnable = value; return *this; }

        /// Set a whole depth stencil state.
        /// Note you can set individual values with their own call
        PipelineMaker& depthStencilState(const VkPipelineDepthStencilStateCreateInfo& value) { depthStencilState_ = value; return *this; }
        PipelineMaker& depthTestEnable(VkBool32 value) { depthStencilState_.depthTestEnable = value; return *this; }
        PipelineMaker& depthWriteEnable(VkBool32 value) { depthStencilState_.depthWriteEnable = value; return *this; }
        PipelineMaker& depthCompareOp(VkCompareOp value) { depthStencilState_.depthCompareOp = value; return *this; }
        PipelineMaker& depthBoundsTestEnable(VkBool32 value) { depthStencilState_.depthBoundsTestEnable = value; return *this; }
        PipelineMaker& stencilTestEnable(VkBool32 value) { depthStencilState_.stencilTestEnable = value; return *this; }
        PipelineMaker& front(VkStencilOpState value) { depthStencilState_.front = value; return *this; }
        PipelineMaker& back(VkStencilOpState value) { depthStencilState_.back = value; return *this; }
        PipelineMaker& minDepthBounds(float value) { depthStencilState_.minDepthBounds = value; return *this; }
        PipelineMaker& maxDepthBounds(float value) { depthStencilState_.maxDepthBounds = value; return *this; }

        /// Set a whole colour blend state.
        /// Note you can set individual values with their own call
        PipelineMaker& colorBlendState(const VkPipelineColorBlendStateCreateInfo& value) { colorBlendState_ = value; return *this; }
        PipelineMaker& logicOpEnable(VkBool32 value) { colorBlendState_.logicOpEnable = value; return *this; }
        PipelineMaker& logicOp(VkLogicOp value) { colorBlendState_.logicOp = value; return *this; }
        PipelineMaker& blendConstants(float r, float g, float b, float a) { float* bc = colorBlendState_.blendConstants; bc[0] = r; bc[1] = g; bc[2] = b; bc[3] = a; return *this; }

        PipelineMaker& dynamicState(VkDynamicState value) { dynamicState_.push_back(value); return *this; }
    private:
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyState_{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
        VkViewport viewport_;
        VkRect2D scissor_;
        VkPipelineRasterizationStateCreateInfo rasterizationState_{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
        VkPipelineMultisampleStateCreateInfo multisampleState_{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
        VkPipelineDepthStencilStateCreateInfo depthStencilState_{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
        VkPipelineColorBlendStateCreateInfo colorBlendState_{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments_;
        std::vector<VkPipelineShaderStageCreateInfo> modules_;
        std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions_;
        std::vector<VkVertexInputBindingDescription> vertexBindingDescriptions_;
        std::vector<VkDynamicState> dynamicState_;
        uint32_t subpass_ = 0;
    };

    /// A class for building compute pipelines.
    class ComputePipelineMaker {
    public:
        ComputePipelineMaker() {
        }

        /// Add a shader module to the pipeline.
        void shader(VkShaderStageFlagBits stage, vku::ShaderModule& shader,
            const char* entryPoint = "main") {
            stage_.module = shader.module();
            stage_.pName = entryPoint;
            stage_.stage = stage;
        }

        /// Add a shader module to the pipeline.
        void shader(VkShaderStageFlagBits stage, VkShaderModule shader,
            const char* entryPoint = "main") {
            stage_.module = shader;
            stage_.pName = entryPoint;
            stage_.stage = stage;
        }

        /// Set the compute shader module.
        ComputePipelineMaker& module(const VkPipelineShaderStageCreateInfo& value) {
            stage_ = value;
            return *this;
        }

        void specializationInfo(VkSpecializationInfo info) {
            info_ = info;
            // Only set the pointer when we actually have specialization info
            stage_.pSpecializationInfo = &info_;
        }

        /// Create a handle to a compute shader.
        VkPipeline create(VkDevice device, const VkPipelineCache& pipelineCache, const VkPipelineLayout& pipelineLayout) {
            VkComputePipelineCreateInfo pipelineInfo{};
            pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;

            pipelineInfo.stage = stage_;
            pipelineInfo.layout = pipelineLayout;

            VkPipeline pipeline;
            VKCHECK(vkCreateComputePipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &pipeline));

            return pipeline;
        }
    private:
        VkPipelineShaderStageCreateInfo stage_{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        VkSpecializationInfo info_;
    };

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
            //VkObjectDestroy<VkDevice, VkDispatchLoaderStatic> deleter(device);

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

    /// Convenience class for updating descriptor sets (uniforms)
    class DescriptorSetUpdater {
    public:
        DescriptorSetUpdater(int maxBuffers = 10, int maxImages = 10, int maxBufferViews = 0) {
            // we must pre-size these buffers as we take pointers to their members.
            bufferInfo_.resize(maxBuffers);
            imageInfo_.resize(maxImages);
            bufferViews_.resize(maxBufferViews);
        }

        /// Call this to begin a new descriptor set.
        void beginDescriptorSet(VkDescriptorSet dstSet) {
            dstSet_ = dstSet;
        }

        /// Call this to begin a new set of images.
        void beginImages(uint32_t dstBinding, uint32_t dstArrayElement, VkDescriptorType descriptorType) {
            VkWriteDescriptorSet wdesc{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            wdesc.dstSet = dstSet_;
            wdesc.dstBinding = dstBinding;
            wdesc.dstArrayElement = dstArrayElement;
            wdesc.descriptorCount = 0;
            wdesc.descriptorType = descriptorType;
            wdesc.pImageInfo = imageInfo_.data() + numImages_;
            descriptorWrites_.push_back(wdesc);
        }

        /// Call this to add a combined image sampler.
        void image(VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout) {
            if (!descriptorWrites_.empty() && (size_t)numImages_ != imageInfo_.size() && descriptorWrites_.back().pImageInfo) {
                descriptorWrites_.back().descriptorCount++;
                imageInfo_[numImages_++] = VkDescriptorImageInfo{ sampler, imageView, imageLayout };
            } else {
                ok_ = false;
            }
        }

        /// Call this to start defining buffers.
        void beginBuffers(uint32_t dstBinding, uint32_t dstArrayElement, VkDescriptorType descriptorType) {
            VkWriteDescriptorSet wdesc{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            wdesc.dstSet = dstSet_;
            wdesc.dstBinding = dstBinding;
            wdesc.dstArrayElement = dstArrayElement;
            wdesc.descriptorCount = 0;
            wdesc.descriptorType = descriptorType;
            wdesc.pBufferInfo = bufferInfo_.data() + numBuffers_;
            descriptorWrites_.push_back(wdesc);
        }

        /// Call this to add a new buffer.
        void buffer(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range) {
            if (!descriptorWrites_.empty() && (size_t)numBuffers_ != bufferInfo_.size() && descriptorWrites_.back().pBufferInfo) {
                descriptorWrites_.back().descriptorCount++;
                bufferInfo_[numBuffers_++] = VkDescriptorBufferInfo{ buffer, offset, range };
            } else {
                ok_ = false;
            }
        }

        /// Call this to start adding buffer views. (for example, writable images).
        void beginBufferViews(uint32_t dstBinding, uint32_t dstArrayElement, VkDescriptorType descriptorType) {
            VkWriteDescriptorSet wdesc{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            wdesc.dstSet = dstSet_;
            wdesc.dstBinding = dstBinding;
            wdesc.dstArrayElement = dstArrayElement;
            wdesc.descriptorCount = 0;
            wdesc.descriptorType = descriptorType;
            wdesc.pTexelBufferView = bufferViews_.data() + numBufferViews_;
            descriptorWrites_.push_back(wdesc);
        }

        /// Call this to add a buffer view. (Texel images)
        void bufferView(VkBufferView view) {
            if (!descriptorWrites_.empty() && (size_t)numBufferViews_ != bufferViews_.size() && descriptorWrites_.back().pImageInfo) {
                descriptorWrites_.back().descriptorCount++;
                bufferViews_[numBufferViews_++] = view;
            } else {
                ok_ = false;
            }
        }

        /// Copy an existing descriptor.
        void copy(VkDescriptorSet srcSet, uint32_t srcBinding, uint32_t srcArrayElement, VkDescriptorSet dstSet, uint32_t dstBinding, uint32_t dstArrayElement, uint32_t descriptorCount) {
            descriptorCopies_.emplace_back(VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET, nullptr, srcSet, srcBinding, srcArrayElement, dstSet, dstBinding, dstArrayElement, descriptorCount);
        }

        /// Call this to update the descriptor sets with their pointers (but not data).
        void update(const VkDevice& device) const {
            vkUpdateDescriptorSets(device, descriptorWrites_.size(), descriptorWrites_.data(), descriptorCopies_.size(), descriptorCopies_.data());
        }

        /// Returns true if the updater is error free.
        bool ok() const { return ok_; }
    private:
        std::vector<VkDescriptorBufferInfo> bufferInfo_;
        std::vector<VkDescriptorImageInfo> imageInfo_;
        std::vector<VkWriteDescriptorSet> descriptorWrites_;
        std::vector<VkCopyDescriptorSet> descriptorCopies_;
        std::vector<VkBufferView> bufferViews_;
        VkDescriptorSet dstSet_;
        int numBuffers_ = 0;
        int numImages_ = 0;
        int numBufferViews_ = 0;
        bool ok_ = true;
    };

    /// A factory class for descriptor set layouts. (An interface to the shaders)
    class DescriptorSetLayoutMaker {
    public:
        DescriptorSetLayoutMaker() {
        }

        void buffer(uint32_t binding, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags, uint32_t descriptorCount) {
            s.bindings.emplace_back(binding, descriptorType, descriptorCount, stageFlags, nullptr);
            s.bindFlags.push_back({});
        }

        void image(uint32_t binding, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags, uint32_t descriptorCount) {
            s.bindings.emplace_back(binding, descriptorType, descriptorCount, stageFlags, nullptr);
            s.bindFlags.push_back({});
        }

        void samplers(uint32_t binding, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags, const std::vector<VkSampler> immutableSamplers) {
            s.samplers.push_back(immutableSamplers);
            auto pImmutableSamplers = s.samplers.back().data();
            s.bindings.emplace_back(binding, descriptorType, (uint32_t)immutableSamplers.size(), stageFlags, pImmutableSamplers);
            s.bindFlags.push_back({});
        }

        void bufferView(uint32_t binding, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags, uint32_t descriptorCount) {
            s.bindings.emplace_back(binding, descriptorType, descriptorCount, stageFlags, nullptr);
            s.bindFlags.push_back({});
        }

        void bindFlag(uint32_t binding, VkDescriptorBindingFlags flags) {
            s.bindFlags[binding] = flags;
            s.useBindFlags = true;
        }

        /// Create a self-deleting descriptor set object.
        VkDescriptorSetLayout create(VkDevice device) const {
            VkDescriptorSetLayoutCreateInfo dsci{};
            dsci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            dsci.bindingCount = (uint32_t)s.bindings.size();
            dsci.pBindings = s.bindings.data();

            VkDescriptorSetLayoutBindingFlagsCreateInfo dslbfci{};
            if (s.useBindFlags) {
                dslbfci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
                dslbfci.bindingCount = (uint32_t)s.bindings.size();
                dslbfci.pBindingFlags = s.bindFlags.data();
                dsci.pNext = &dslbfci;
            }

            VkDescriptorSetLayout layout;
            VKCHECK(vkCreateDescriptorSetLayout(device, &dsci, nullptr, &layout));
            return layout;
        }

    private:
        struct State {
            std::vector<VkDescriptorSetLayoutBinding> bindings;
            std::vector<std::vector<VkSampler> > samplers;
            int numSamplers = 0;
            bool useBindFlags = false;
            std::vector<VkDescriptorBindingFlags> bindFlags;
        };

        State s;
    };

    /// A factory class for descriptor sets (A set of uniform bindings)
    class DescriptorSetMaker {
    public:
        // Construct a new, empty DescriptorSetMaker.
        DescriptorSetMaker() {
        }

        /// Add another layout describing a descriptor set.
        void layout(VkDescriptorSetLayout layout) {
            s.layouts.push_back(layout);
        }

        /// Allocate a vector of non-self-deleting descriptor sets
        /// Note: descriptor sets get freed with the pool, so this is the better choice.
        std::vector<VkDescriptorSet> create(VkDevice device, VkDescriptorPool descriptorPool) const {
            VkDescriptorSetAllocateInfo dsai{};
            dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            dsai.descriptorPool = descriptorPool;
            dsai.descriptorSetCount = (uint32_t)s.layouts.size();
            dsai.pSetLayouts = s.layouts.data();

            std::vector<VkDescriptorSet> descriptorSets;
            descriptorSets.resize(s.layouts.size());

            VKCHECK(vkAllocateDescriptorSets(device, &dsai, descriptorSets.data()));

            return descriptorSets;
        }

    private:
        struct State {
            std::vector<VkDescriptorSetLayout> layouts;
        };

        State s;
    };

    /// Generic image with a view and memory object.
    /// Vulkan images need a memory object to hold the data and a view object for the GPU to access the data.
    class GenericImage {
    public:
        GenericImage() {
        }

        GenericImage(VkDevice device, VmaAllocator allocator, const VkImageCreateInfo& info, VkImageViewType viewType, VkImageAspectFlags aspectMask, bool makeHostImage, const char* debugName = nullptr) {
            create(device, allocator, info, viewType, aspectMask, makeHostImage, debugName);
        }

        GenericImage(GenericImage const&) = delete;

        GenericImage(GenericImage&& other) noexcept {
            s = other.s;
            assert(!other.s.destroyed);
            other.s.destroyed = true;
            s.destroyed = false;
        }

        GenericImage& operator=(GenericImage&& other) noexcept {
            s = other.s;
            assert(!other.s.destroyed);
            other.s.destroyed = true;
            s.destroyed = false;
            return *this;
        }

        VkImage image() const { assert(!s.destroyed);  return s.image; }
        VkImageView imageView() const { assert(!s.destroyed);  return s.imageView; }

        /// Clear the colour of an image.
        void clear(VkCommandBuffer cb, const std::array<float, 4> colour = { 1, 1, 1, 1 }) {
            assert(!s.destroyed);
            setLayout(cb, ImageLayout::TransferDstOptimal);
            VkClearColorValue ccv;
            ccv.float32[0] = colour[0];
            ccv.float32[1] = colour[1];
            ccv.float32[2] = colour[2];
            ccv.float32[3] = colour[3];

            VkImageSubresourceRange range{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

            vkCmdClearColorImage(cb, s.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &ccv, 1, &range);
        }

        /// Update the image with an array of pixels. (Currently 2D only)
        void update(VkDevice device, const void* data, VkDeviceSize bytesPerPixel) {
            const uint8_t* src = (const uint8_t*)data;
            for (uint32_t mipLevel = 0; mipLevel != info().mipLevels; ++mipLevel) {
                // Array images are layed out horizontally. eg. [left][front][right] etc.
                for (uint32_t arrayLayer = 0; arrayLayer != info().arrayLayers; ++arrayLayer) {
                    VkImageSubresource subresource{ VK_IMAGE_ASPECT_COLOR_BIT, mipLevel, arrayLayer };
                    VkSubresourceLayout srLayout;
                    vkGetImageSubresourceLayout(device, s.image, &subresource, &srLayout);

                    uint8_t* dest;
                    vmaMapMemory(s.allocator, s.allocation, (void**)&dest);
                    dest += srLayout.offset;
                    size_t bytesPerLine = s.info.extent.width * bytesPerPixel;
                    size_t srcStride = bytesPerLine * info().arrayLayers;
                    for (uint32_t y = 0; y != s.info.extent.height; ++y) {
                        memcpy(dest, src + arrayLayer * bytesPerLine, bytesPerLine);
                        src += srcStride;
                        dest += srLayout.rowPitch;
                    }
                    vmaUnmapMemory(s.allocator, s.allocation);
                }
            }
        }

        /// Copy another image to this one. This also changes the layout.
        void copy(VkCommandBuffer cb, vku::GenericImage& srcImage) {
            srcImage.setLayout(cb, ImageLayout::TransferSrcOptimal);
            setLayout(cb, ImageLayout::TransferDstOptimal);
            for (uint32_t mipLevel = 0; mipLevel != info().mipLevels; ++mipLevel) {
                VkImageCopy region{};
                region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, mipLevel, 0, 1 };
                region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, mipLevel, 0, 1 };
                region.extent = s.info.extent;
                vkCmdCopyImage(cb, srcImage.image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, s.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
            }
        }

        /// Copy a subimage in a buffer to this image.
        void copy(VkCommandBuffer cb, VkBuffer buffer, uint32_t mipLevel, uint32_t arrayLayer, uint32_t width, uint32_t height, uint32_t depth, uint32_t offset) {
            setLayout(cb, ImageLayout::TransferDstOptimal, PipelineStageFlags::Transfer, AccessFlags::TransferWrite);
            VkBufferImageCopy region{};
            region.bufferOffset = offset;
            VkExtent3D extent;
            extent.width = width;
            extent.height = height;
            extent.depth = depth;
            region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, mipLevel, arrayLayer, 1 };
            region.imageExtent = extent;
            vkCmdCopyBufferToImage(cb, buffer, s.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        }

        void upload(VkDevice device, VmaAllocator allocator, std::vector<uint8_t>& bytes, VkCommandPool commandPool, VkPhysicalDeviceMemoryProperties memprops, VkQueue queue, uint32_t numMips = ~0u) {
            UNUSED(memprops);
            vku::GenericBuffer stagingBuffer(device, allocator, (VkBufferUsageFlags)VK_BUFFER_USAGE_TRANSFER_SRC_BIT, (VkDeviceSize)bytes.size(), VMA_MEMORY_USAGE_CPU_ONLY);
            stagingBuffer.updateLocal(device, (const void*)bytes.data(), bytes.size());

            if (numMips == ~0u)
                numMips = s.info.mipLevels;

            // Copy the staging buffer to the GPU texture and set the layout.
            vku::executeImmediately(device, commandPool, queue, [&](VkCommandBuffer cb) {
                auto bp = getBlockParams(s.info.format);
                VkBuffer buf = stagingBuffer.buffer();
                uint32_t offset = 0;
                for (uint32_t mipLevel = 0; mipLevel != numMips; ++mipLevel) {
                    auto width = mipScale(s.info.extent.width, mipLevel);
                    auto height = mipScale(s.info.extent.height, mipLevel);
                    auto depth = mipScale(s.info.extent.depth, mipLevel);
                    for (uint32_t face = 0; face != s.info.arrayLayers; ++face) {
                        copy(cb, buf, mipLevel, face, width, height, depth, offset);
                        offset += ((bp.bytesPerBlock + 3) & ~3) * ((width / bp.blockWidth) * (height / bp.blockHeight));
                    }
                }
                setLayout(cb, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
                });
        }

        void upload(VkDevice device, VmaAllocator allocator, std::vector<uint8_t>& bytes, VkPhysicalDeviceMemoryProperties memprops, VkCommandBuffer cb, uint32_t numMips = ~0u) {
            UNUSED(memprops);
            vku::GenericBuffer stagingBuffer(device, allocator, (VkBufferUsageFlags)VK_BUFFER_USAGE_TRANSFER_SRC_BIT, (VkDeviceSize)bytes.size(), VMA_MEMORY_USAGE_CPU_ONLY);
            stagingBuffer.updateLocal(device, (const void*)bytes.data(), bytes.size());

            // Copy the staging buffer to the GPU texture and set the layout.
            {
                if (numMips == ~0u)
                    numMips = s.info.mipLevels;
                auto bp = getBlockParams(s.info.format);
                VkBuffer buf = stagingBuffer.buffer();
                uint32_t offset = 0;
                for (uint32_t mipLevel = 0; mipLevel != numMips; ++mipLevel) {
                    auto width = mipScale(s.info.extent.width, mipLevel);
                    auto height = mipScale(s.info.extent.height, mipLevel);
                    auto depth = mipScale(s.info.extent.depth, mipLevel);
                    for (uint32_t face = 0; face != s.info.arrayLayers; ++face) {
                        copy(cb, buf, mipLevel, face, width, height, depth, offset);
                        offset += ((bp.bytesPerBlock + 3) & ~3) * ((width / bp.blockWidth) * (height / bp.blockHeight));
                    }
                }
                setLayout(cb, ImageLayout::ShaderReadOnlyOptimal);
            }
        }

        /// Change the layout of this image using a memory barrier.
        void setLayout(VkCommandBuffer cb, ImageLayout newLayout, VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT) {
            if (newLayout == s.currentLayout) return;
            ImageLayout oldLayout = s.currentLayout;
            s.currentLayout = newLayout;

            VkImageMemoryBarrier imageMemoryBarriers{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            imageMemoryBarriers.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageMemoryBarriers.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageMemoryBarriers.oldLayout = oldLayout;
            imageMemoryBarriers.newLayout = ImageLayout(newLayout);
            imageMemoryBarriers.image = s.image;
            imageMemoryBarriers.subresourceRange = { aspectMask, 0, s.info.mipLevels, 0, s.info.arrayLayers };

            // Put barrier on top
            VkPipelineStageFlags srcStageMask{ VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT };
            VkPipelineStageFlags dstStageMask{ VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT };
            VkDependencyFlags dependencyFlags{};
            VkAccessFlags srcMask{};
            VkAccessFlags dstMask = s.lastUsageAccessFlags;


            typedef ImageLayout::Bits il;
            typedef AccessFlags::Bits afb;
            typedef PipelineStageFlags::Bits psfb;

            switch (newLayout.value) {
            case il::Undefined: break;
            case il::General: dstMask = afb::TransferWrite; dstStageMask = psfb::Transfer; break;
            case il::ColorAttachmentOptimal: dstMask = afb::ColorAttachmentWrite; dstStageMask = psfb::ColorAttachmentOutput; break;
            case il::DepthStencilAttachmentOptimal: dstMask = afb::DepthStencilAttachmentWrite; dstStageMask = psfb::EarlyFragmentTests; break;
            case il::DepthStencilReadOnlyOptimal: dstMask = afb::DepthStencilAttachmentRead; dstStageMask = psfb::EarlyFragmentTests; break;
            case il::ShaderReadOnlyOptimal: dstMask = afb::ShaderRead; dstStageMask = psfb::VertexShader; break;
            case il::TransferSrcOptimal: dstMask = afb::TransferRead; dstStageMask = psfb::Transfer; break;
            case il::TransferDstOptimal: dstMask = afb::TransferWrite; dstStageMask = psfb::Transfer; break;
            case il::Preinitialized: dstMask = afb::TransferWrite; dstStageMask = psfb::Transfer; break;
            case il::PresentSrcKHR: dstMask = afb::MemoryRead; break;
            default: break;
            }
            //printf("%08x %08x\n", (VkFlags)srcMask, (VkFlags)dstMask);

            imageMemoryBarriers.srcAccessMask = srcMask;
            imageMemoryBarriers.dstAccessMask = dstMask;

            s.lastUsageAccessFlags = dstMask;
            s.lastUsageStage = dstStageMask;

            vkCmdPipelineBarrier(cb, srcStageMask, dstStageMask, dependencyFlags, 0, nullptr, 0, nullptr, 1, &imageMemoryBarriers);
        }

        void barrier(VkCommandBuffer& cb, VkPipelineStageFlags fromPS, VkPipelineStageFlags toPS, VkAccessFlags fromAF, VkAccessFlags toAF) {
            VkImageMemoryBarrier imb{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            imb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imb.srcAccessMask = fromAF;
            imb.dstAccessMask = toAF;
            imb.newLayout = s.currentLayout;
            imb.oldLayout = s.currentLayout;
            imb.subresourceRange = { s.aspectFlags, 0, s.info.mipLevels, 0, s.info.arrayLayers };
            imb.image = s.image;
            s.lastUsageAccessFlags = toAF;
            s.lastUsageStage = toPS;

            vkCmdPipelineBarrier(
                cb,
                fromPS, toPS,
                VK_DEPENDENCY_BY_REGION_BIT,
                0, nullptr,
                0, nullptr,
                1, &imb
            );
        }

        void setLayout(VkCommandBuffer cb, ImageLayout newLayout, PipelineStageFlags srcStageMask, PipelineStageFlags dstStageMask, AccessFlags srcMask, AccessFlags dstMask, VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT) {
            if (newLayout == s.currentLayout) return;
            VkImageLayout oldLayout = s.currentLayout;
            s.currentLayout = newLayout;
            s.lastUsageAccessFlags = dstMask;
            s.lastUsageStage = dstStageMask;

            VkImageMemoryBarrier imageMemoryBarriers{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            imageMemoryBarriers.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageMemoryBarriers.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageMemoryBarriers.oldLayout = oldLayout;
            imageMemoryBarriers.newLayout = newLayout;
            imageMemoryBarriers.image = s.image;
            imageMemoryBarriers.subresourceRange = { aspectMask, 0, s.info.mipLevels, 0, s.info.arrayLayers };

            // Put barrier on top
            imageMemoryBarriers.srcAccessMask = srcMask;
            imageMemoryBarriers.dstAccessMask = dstMask;

            vkCmdPipelineBarrier(cb,
                srcStageMask, dstStageMask,
                VK_DEPENDENCY_BY_REGION_BIT,
                0, nullptr,
                0, nullptr,
                1, &imageMemoryBarriers);
        }

        void setLayout(VkCommandBuffer cmdBuf, ImageLayout newLayout, PipelineStageFlags newStage, AccessFlags newAccess) {
            setLayout(cmdBuf, newLayout, s.lastUsageStage, newStage, s.lastUsageAccessFlags, newAccess);
        }

        /// Set what the image thinks is its current layout (ie. the old layout in an image barrier).
        void setCurrentLayout(VkImageLayout oldLayout, VkPipelineStageFlags lastPipelineStage, VkAccessFlags lastAccess) {
            s.currentLayout = oldLayout;
            s.lastUsageAccessFlags = lastAccess;
            s.lastUsageStage = lastPipelineStage;
        }

        VkFormat format() const { return s.info.format; }
        VkExtent3D extent() const { return s.info.extent; }
        const VkImageCreateInfo& info() const { return s.info; }
        ImageLayout layout() const { return s.currentLayout; }

        void* map() {
            assert(!s.destroyed);
            void* res;
            vmaMapMemory(s.allocator, s.allocation, &res);
            return res;
        }

        void unmap() {
            assert(!s.destroyed);
            vmaUnmapMemory(s.allocator, s.allocation);
        }

        void destroy() {
            if (s.destroyed) return;
            if (s.image) {
                if (s.imageView) {
                    vkDestroyImageView(s.device, s.imageView, nullptr);
                }

                vkDestroyImage(s.device, s.image, nullptr);

                vmaFreeMemory(s.allocator, s.allocation);
            }
            s.destroyed = true;
        }

        ~GenericImage() {
            destroy();
        }
    protected:
        void create(VkDevice device, VmaAllocator allocator, const VkImageCreateInfo& info, VkImageViewType viewType, VkImageAspectFlags aspectMask, bool hostImage, const char* debugName) {
            s.device = device;
            s.allocator = allocator;
            s.currentLayout = info.initialLayout;
            s.info = info;
            s.aspectFlags = aspectMask;
            s.device = device;
            VKCHECK(vkCreateImage(device, &info, nullptr, &s.image));

            if (debugName) {
                VkDebugUtilsObjectNameInfoEXT nameInfo;
                nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
                nameInfo.pObjectName = debugName;
                nameInfo.objectHandle = (uint64_t)s.image;
                nameInfo.objectType = VK_OBJECT_TYPE_IMAGE;
                nameInfo.pNext = nullptr;
                auto setObjName = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT");
                setObjName(device, &nameInfo);
            }

            VmaAllocationCreateInfo vaci{};
            vaci.usage = hostImage ? VMA_MEMORY_USAGE_CPU_ONLY : VMA_MEMORY_USAGE_GPU_ONLY;
            vaci.memoryTypeBits = 0;
            vaci.pool = VK_NULL_HANDLE;
            vaci.preferredFlags = 0;
            vaci.requiredFlags = 0;
            vaci.memoryTypeBits = 0;
            vaci.pUserData = (char*)debugName;
            vaci.flags = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
            VmaAllocationInfo vci;
            VkResult allocResult =
                vmaAllocateMemoryForImage(allocator, s.image, &vaci, &s.allocation, &vci);

            if (allocResult != VK_SUCCESS) {
                fatalErr("Vulkan memory allocation failed");
            }

            VKCHECK(vkBindImageMemory(device, s.image, vci.deviceMemory, vci.offset));

            const ImageUsageFlags viewFlagBits =
                ImageUsageFlags::Sampled |
                ImageUsageFlags::Storage |
                ImageUsageFlags::ColorAttachment |
                ImageUsageFlags::DepthStencilAttachment |
                ImageUsageFlags::InputAttachment;

            if (!hostImage && (uint32_t)(ImageUsageFlags(info.usage) & viewFlagBits) != 0) {
                VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
                viewInfo.image = s.image;
                viewInfo.viewType = viewType;
                viewInfo.format = info.format;
                viewInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
                viewInfo.subresourceRange = VkImageSubresourceRange{ aspectMask, 0, info.mipLevels, 0, info.arrayLayers };
                VKCHECK(vkCreateImageView(device, &viewInfo, nullptr, &s.imageView));

                if (debugName) {
                    VkDebugUtilsObjectNameInfoEXT nameInfo;
                    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
                    nameInfo.pObjectName = debugName;
                    nameInfo.objectHandle = (uint64_t)s.imageView;
                    nameInfo.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
                    nameInfo.pNext = nullptr;
                    auto setObjName = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT");
                    setObjName(device, &nameInfo);
                }
            }
        }

        struct State {
            VkImage image;
            VkImageView imageView = VK_NULL_HANDLE;
            VkDevice device = VK_NULL_HANDLE;
            VkDeviceSize size = 0;
            ImageLayout currentLayout = ImageLayout::Undefined;
            VkPipelineStageFlags lastUsageStage = PipelineStageFlags::TopOfPipe;
            VkAccessFlags lastUsageAccessFlags = AccessFlags::MemoryWrite;
            VkImageAspectFlags aspectFlags;
            VkImageCreateInfo info;
            VmaAllocation allocation = nullptr;
            VmaAllocator allocator;
            bool destroyed = false;
        };

        State s;
    };


    /// A 2D texture image living on the GPU or a staging buffer visible to the CPU.
    class TextureImage2D : public GenericImage {
    public:
        TextureImage2D() {
        }

        TextureImage2D(VkDevice device, VmaAllocator allocator, uint32_t width, uint32_t height, uint32_t mipLevels = 1, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM, bool hostImage = false, const char* debugName = nullptr) {
            VkImageCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            info.flags = {};
            info.imageType = VK_IMAGE_TYPE_2D;
            info.format = format;
            info.extent = VkExtent3D{ width, height, 1U };
            info.mipLevels = mipLevels;
            info.arrayLayers = 1;
            info.samples = VK_SAMPLE_COUNT_1_BIT;
            info.tiling = hostImage ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
            info.usage = ImageUsageFlags::Sampled | ImageUsageFlags::TransferSrc | ImageUsageFlags::TransferDst;
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            info.queueFamilyIndexCount = 0;
            info.pQueueFamilyIndices = nullptr;
            info.initialLayout = ImageLayout(hostImage ? ImageLayout::Preinitialized : ImageLayout::Undefined);
            create(device, allocator, info, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, hostImage, debugName);
        }
    private:
    };

    /// A cube map texture image living on the GPU or a staging buffer visible to the CPU.
    class TextureImageCube : public GenericImage {
    public:
        TextureImageCube() {
        }

        TextureImageCube(VkDevice device, VmaAllocator allocator, uint32_t width, uint32_t height, uint32_t mipLevels = 1, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM, bool hostImage = false, const char* debugName = nullptr, VkImageUsageFlags usageFlags = ImageUsageFlags::Sampled | ImageUsageFlags::TransferSrc | ImageUsageFlags::TransferDst | ImageUsageFlags::Storage) {
            VkImageCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            info.flags = { VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT };
            info.imageType = VK_IMAGE_TYPE_2D;
            info.format = format;
            info.extent = VkExtent3D{ width, height, 1U };
            info.mipLevels = mipLevels;
            info.arrayLayers = 6;
            info.samples = VK_SAMPLE_COUNT_1_BIT;
            info.tiling = hostImage ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
            info.usage = usageFlags;
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            info.queueFamilyIndexCount = 0;
            info.pQueueFamilyIndices = nullptr;
            //info.initialLayout = hostImage ? VkImageLayout::ePreinitialized : VkImageLayout::eUndefined;
            info.initialLayout = ImageLayout(ImageLayout::Preinitialized);
            create(device, allocator, info, VK_IMAGE_VIEW_TYPE_CUBE, VK_IMAGE_ASPECT_COLOR_BIT, hostImage, debugName);
        }
    private:
    };

    /// An image to use as a depth buffer on a renderpass.
    class DepthStencilImage : public GenericImage {
    public:
        DepthStencilImage() {
        }

        DepthStencilImage(VkDevice device, VmaAllocator allocator, uint32_t width, uint32_t height, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT, VkFormat format = VK_FORMAT_D24_UNORM_S8_UINT, const char* debugName = nullptr) {
            VkImageCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            info.flags = {};

            info.imageType = VK_IMAGE_TYPE_2D;
            info.format = format;
            info.extent = VkExtent3D{ width, height, 1U };
            info.mipLevels = 1;
            info.arrayLayers = 1;
            info.samples = samples;
            info.tiling = VK_IMAGE_TILING_OPTIMAL;
            info.usage = ImageUsageFlags::DepthStencilAttachment | ImageUsageFlags::TransferSrc | ImageUsageFlags::Sampled;
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            info.queueFamilyIndexCount = 0;
            info.pQueueFamilyIndices = nullptr;
            info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            create(device, allocator, info, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT, false, debugName);
        }
    private:
    };

    /// An image to use as a colour buffer on a renderpass.
    class ColorAttachmentImage : public GenericImage {
    public:
        ColorAttachmentImage() {
        }

        ColorAttachmentImage(VkDevice device, VmaAllocator allocator, uint32_t width, uint32_t height, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM, const char* debugName = nullptr) {
            VkImageCreateInfo info{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };

            info.imageType = VK_IMAGE_TYPE_2D;
            info.format = format;
            info.extent = VkExtent3D{ width, height, 1U };
            info.mipLevels = 1;
            info.arrayLayers = 1;
            info.samples = VK_SAMPLE_COUNT_1_BIT;
            info.tiling = VK_IMAGE_TILING_OPTIMAL;
            info.usage = ImageUsageFlags::ColorAttachment | ImageUsageFlags::TransferSrc | ImageUsageFlags::Sampled;
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            info.queueFamilyIndexCount = 0;
            info.pQueueFamilyIndices = nullptr;
            info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            create(device, allocator, info, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, false, debugName);
        }
    private:
    };

    /// A class to help build samplers.
    /// Samplers tell the shader stages how to sample an image.
    /// They are used in combination with an image to make a combined image sampler
    /// used by texture() calls in shaders.
    /// They can also be passed to shaders directly for use on multiple image sources.
    class SamplerMaker {
    public:
        /// Default to a very basic sampler.
        SamplerMaker() {
            s.info.magFilter = VK_FILTER_NEAREST;
            s.info.minFilter = VK_FILTER_NEAREST;
            s.info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            s.info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            s.info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            s.info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            s.info.mipLodBias = 0.0f;
            s.info.anisotropyEnable = 0;
            s.info.maxAnisotropy = 0.0f;
            s.info.compareEnable = 0;
            s.info.compareOp = VK_COMPARE_OP_NEVER;
            s.info.minLod = 0;
            s.info.maxLod = 0;
            s.info.borderColor = VkBorderColor{};
            s.info.unnormalizedCoordinates = 0;
        }

        ////////////////////
        //
        // Setters
        //
        SamplerMaker& flags(VkSamplerCreateFlags value) { s.info.flags = value; return *this; }

        /// Set the magnify filter value. (for close textures)
        /// Options are: VK_FILTER_LINEAR and VkFilter::eNearest
        SamplerMaker& magFilter(VkFilter value) { s.info.magFilter = value; return *this; }

        /// Set the minnify filter value. (for far away textures)
        /// Options are: VK_FILTER_LINEAR and VkFilter::eNearest
        SamplerMaker& minFilter(VkFilter value) { s.info.minFilter = value; return *this; }

        /// Set the minnify filter value. (for far away textures)
        /// Options are: VK_SAMPLER_MIPMAP_MODE_LINEAR and VkSamplerMipmapMode::eNearest
        SamplerMaker& mipmapMode(VkSamplerMipmapMode value) { s.info.mipmapMode = value; return *this; }
        SamplerMaker& addressModeU(VkSamplerAddressMode value) { s.info.addressModeU = value; return *this; }
        SamplerMaker& addressModeV(VkSamplerAddressMode value) { s.info.addressModeV = value; return *this; }
        SamplerMaker& addressModeW(VkSamplerAddressMode value) { s.info.addressModeW = value; return *this; }
        SamplerMaker& mipLodBias(float value) { s.info.mipLodBias = value; return *this; }
        SamplerMaker& anisotropyEnable(VkBool32 value) { s.info.anisotropyEnable = value; return *this; }
        SamplerMaker& maxAnisotropy(float value) { s.info.maxAnisotropy = value; return *this; }
        SamplerMaker& compareEnable(VkBool32 value) { s.info.compareEnable = value; return *this; }
        SamplerMaker& compareOp(VkCompareOp value) { s.info.compareOp = value; return *this; }
        SamplerMaker& minLod(float value) { s.info.minLod = value; return *this; }
        SamplerMaker& maxLod(float value) { s.info.maxLod = value; return *this; }
        SamplerMaker& borderColor(VkBorderColor value) { s.info.borderColor = value; return *this; }
        SamplerMaker& unnormalizedCoordinates(VkBool32 value) { s.info.unnormalizedCoordinates = value; return *this; }

        /// Allocate a non self-deleting Sampler.
        VkSampler create(VkDevice device) const {
            VkSampler sampler;
            VKCHECK(vkCreateSampler(device, &s.info, nullptr, &sampler));
            return sampler;
        }

    private:
        struct State {
            VkSamplerCreateInfo info{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        };

        State s;
    };

    inline void transitionLayout(VkCommandBuffer& cb, VkImage img, ImageLayout oldLayout, ImageLayout newLayout, PipelineStageFlags srcStageMask, PipelineStageFlags dstStageMask, AccessFlags srcMask, AccessFlags dstMask, VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT) {
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
