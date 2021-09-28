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
#include <vector>
#include <functional>
#include <cstddef>
#include <Libs/volk.h>
#include <vk_mem_alloc.h>
#include <assert.h>

#include <Core/Log.hpp>
#include <Core/Fatal.hpp>
// just in case something pulls in windows.h
#undef min
#undef max

namespace worlds {
    typedef uint32_t AssetID;
}

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

    const char* toString(VkPhysicalDeviceType type);
    const char* toString(VkMemoryPropertyFlags flags);
    const char* toString(VkResult result);

    inline VkResult checkVkResult(VkResult result, const char* file, int line) {
        switch (result) {
        case VK_ERROR_OUT_OF_HOST_MEMORY:
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        case VK_ERROR_DEVICE_LOST:
            fatalErrInternal(vku::toString(result), file, line);
            break;
        default: return result;
        }

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

    // =============================
    // Buffers
    // =============================

    /// A generic buffer that may be used as a vertex buffer, uniform buffer or other kinds of memory resident data.
    /// Buffers require memory objects which represent GPU and CPU resources.
    class GenericBuffer {
    public:
        GenericBuffer();

        GenericBuffer(VkDevice device, VmaAllocator allocator, VkBufferUsageFlags usage, VkDeviceSize size, VmaMemoryUsage memUsage = VMA_MEMORY_USAGE_GPU_ONLY, const char* debugName = nullptr);

        /// For a host visible buffer, copy memory to the buffer object.
        void updateLocal(const VkDevice& device, const void* value, VkDeviceSize size) const;

        /// For a purely device local buffer, copy memory to the buffer object immediately.
        /// Note that this will stall the pipeline!
        void upload(VkDevice device, VkCommandPool commandPool, VkQueue queue, const void* value, VkDeviceSize size) const;

        template<typename T>
        void upload(VkDevice device, VkCommandPool commandPool, VkQueue queue, const std::vector<T>& value) const {
            upload(device, commandPool, queue, value.data(), value.size() * sizeof(T));
        }

        template<typename T>
        void upload(VkDevice device, VkCommandPool commandPool, VkQueue queue, const T& value) const {
            upload(device, commandPool, queue, &value, sizeof(value));
        }

        void barrier(VkCommandBuffer cb, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, uint32_t srcQueueFamilyIndex, uint32_t dstQueueFamilyIndex) const;

        template<class Type, class Allocator>
        void updateLocal(const VkDevice& device, const std::vector<Type, Allocator>& value) const {
            updateLocal(device, (void*)value.data(), VkDeviceSize(value.size() * sizeof(Type)));
        }

        template<class Type>
        void updateLocal(const VkDevice& device, const Type& value) const {
            updateLocal(device, (void*)&value, VkDeviceSize(sizeof(Type)));
        }

        void* map(const VkDevice& device) const;
        void unmap(const VkDevice& device) const;
        void flush(const VkDevice& device) const;
        void invalidate(const VkDevice& device) const;

        GenericBuffer(GenericBuffer const&) = delete;

        GenericBuffer(GenericBuffer&& other) noexcept;

        GenericBuffer& operator=(GenericBuffer&& other) noexcept;

        VkBuffer buffer() const { return buffer_; }
        VkDeviceSize size() const { return size_; }

        void destroy();

        ~GenericBuffer();

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

    // =============================
    // Images
    // =============================

    /// Generic image with a view and memory object.
    /// Vulkan images need a memory object to hold the data and a view object for the GPU to access the data.
    class GenericImage {
    public:
        GenericImage();

        GenericImage(VkDevice device, VmaAllocator allocator, const VkImageCreateInfo& info, VkImageViewType viewType, VkImageAspectFlags aspectMask, bool makeHostImage, const char* debugName = nullptr);

        GenericImage(GenericImage const&) = delete;

        GenericImage(GenericImage&& other) noexcept;

        GenericImage& operator=(GenericImage&& other) noexcept;

        VkImage image() const { assert(!s.destroyed);  return s.image; }
        VkImageView imageView() const { assert(!s.destroyed);  return s.imageView; }

        /// Clear the colour of an image.
        void clear(VkCommandBuffer cb, const std::array<float, 4> colour = { 1, 1, 1, 1 });

        /// Update the image with an array of pixels. (Currently 2D only)
        void update(VkDevice device, const void* data, VkDeviceSize bytesPerPixel);

        /// Copy another image to this one. This also changes the layout.
        void copy(VkCommandBuffer cb, vku::GenericImage& srcImage);

        /// Copy a subimage in a buffer to this image.
        void copy(VkCommandBuffer cb, VkBuffer buffer, uint32_t mipLevel, uint32_t arrayLayer, uint32_t width, uint32_t height, uint32_t depth, uint32_t offset);

        void upload(VkDevice device, VmaAllocator allocator, std::vector<uint8_t>& bytes, VkCommandPool commandPool, VkPhysicalDeviceMemoryProperties memprops, VkQueue queue, uint32_t numMips = ~0u);

        void upload(VkDevice device, VmaAllocator allocator, std::vector<uint8_t>& bytes, VkPhysicalDeviceMemoryProperties memprops, VkCommandBuffer cb, uint32_t numMips = ~0u);

        /// Change the layout of this image using a memory barrier.
        void setLayout(VkCommandBuffer cb, VkImageLayout newLayout, VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT);

        void barrier(VkCommandBuffer& cb, VkPipelineStageFlags fromPS, VkPipelineStageFlags toPS, VkAccessFlags fromAF, VkAccessFlags toAF);

        void setLayout(VkCommandBuffer cb, VkImageLayout newLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcMask, VkAccessFlags dstMask, VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT);

        void setLayout(VkCommandBuffer cmdBuf, VkImageLayout newLayout, VkPipelineStageFlags newStage, VkAccessFlags newAccess, VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT) {
            setLayout(cmdBuf, newLayout, s.lastUsageStage, newStage, s.lastUsageAccessFlags, newAccess, aspectMask);
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
        VkImageLayout layout() const { return s.currentLayout; }

        void* map();
        void unmap();

        void destroy();

        ~GenericImage() {
            destroy();
        }
    protected:
        void create(VkDevice device, VmaAllocator allocator, const VkImageCreateInfo& info, VkImageViewType viewType, VkImageAspectFlags aspectMask, bool hostImage, const char* debugName);

        struct State {
            VkImage image;
            VkImageView imageView = VK_NULL_HANDLE;
            VkDevice device = VK_NULL_HANDLE;
            VkDeviceSize size = 0;
            VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            VkPipelineStageFlags lastUsageStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            VkAccessFlags lastUsageAccessFlags = VK_ACCESS_MEMORY_WRITE_BIT;
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
        TextureImage2D();
        TextureImage2D(VkDevice device, VmaAllocator allocator, uint32_t width, uint32_t height, uint32_t mipLevels = 1, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM, bool hostImage = false, const char* debugName = nullptr);
    };

    /// A cube map texture image living on the GPU or a staging buffer visible to the CPU.
    class TextureImageCube : public GenericImage {
    public:
        TextureImageCube();

        TextureImageCube(VkDevice device, VmaAllocator allocator, uint32_t width, uint32_t height, uint32_t mipLevels = 1, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM, bool hostImage = false, const char* debugName = nullptr, VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
    };

    /// An image to use as a depth buffer on a renderpass.
    class DepthStencilImage : public GenericImage {
    public:
        DepthStencilImage();

        DepthStencilImage(VkDevice device, VmaAllocator allocator, uint32_t width, uint32_t height, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT, VkFormat format = VK_FORMAT_D24_UNORM_S8_UINT, const char* debugName = nullptr);
    };

    /// An image to use as a colour buffer on a renderpass.
    class ColorAttachmentImage : public GenericImage {
    public:
        ColorAttachmentImage();

        ColorAttachmentImage(VkDevice device, VmaAllocator allocator, uint32_t width, uint32_t height, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM, const char* debugName = nullptr);
    };

    // =============================
    // Pipeline utils 
    // =============================

    /// A class for building pipeline layouts.
    /// Pipeline layouts describe the descriptor sets and push constants used by the shaders.
    class PipelineLayoutMaker {
    public:
        PipelineLayoutMaker();

        /// Create a pipeline layout object.
        VkPipelineLayout create(VkDevice device) const;

        /// Add a descriptor set layout to the pipeline.
        void descriptorSetLayout(VkDescriptorSetLayout layout);

        /// Add a push constant range to the pipeline.
        /// These describe the size and location of variables in the push constant area.
        void pushConstantRange(VkShaderStageFlags stageFlags_, uint32_t offset_, uint32_t size_);

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
        PipelineMaker(uint32_t width, uint32_t height);

        VkPipeline create(VkDevice device,
            const VkPipelineCache& pipelineCache,
            const VkPipelineLayout& pipelineLayout,
            const VkRenderPass& renderPass, bool defaultBlend = true);

        /// Add a shader module to the pipeline.
        void shader(VkShaderStageFlagBits stage, vku::ShaderModule& shader,
            const char* entryPoint = "main", VkSpecializationInfo* pSpecializationInfo = nullptr);

        /// Add a shader module to the pipeline.
        void shader(VkShaderStageFlagBits stage, VkShaderModule shader,
            const char* entryPoint = "main", VkSpecializationInfo* pSpecializationInfo = nullptr);

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
        void blendBegin(VkBool32 enable);

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
        ComputePipelineMaker();

        /// Add a shader module to the pipeline.
        void shader(VkShaderStageFlagBits stage, vku::ShaderModule& shader,
            const char* entryPoint = "main");

        /// Add a shader module to the pipeline.
        void shader(VkShaderStageFlagBits stage, VkShaderModule shader,
            const char* entryPoint = "main");

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
        VkPipeline create(VkDevice device, const VkPipelineCache& pipelineCache, const VkPipelineLayout& pipelineLayout);
    private:
        VkPipelineShaderStageCreateInfo stage_{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        VkSpecializationInfo info_;
    };

    /// A class to help build samplers.
    /// Samplers tell the shader stages how to sample an image.
    /// They are used in combination with an image to make a combined image sampler
    /// used by texture() calls in shaders.
    /// They can also be passed to shaders directly for use on multiple image sources.
    class SamplerMaker {
    public:
        /// Default to a very basic sampler.
        SamplerMaker();

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
        VkSampler create(VkDevice device) const;

    private:
        struct State {
            VkSamplerCreateInfo info{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        };

        State s;
    };

    void transitionLayout(VkCommandBuffer& cb, VkImage img, VkImageLayout oldLayout, VkImageLayout newLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcMask, VkAccessFlags dstMask, VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT);

    VkSampleCountFlagBits sampleCountFlags(int sampleCount);
    VkClearValue makeColorClearValue(float r, float g, float b, float a);
    VkClearValue makeDepthStencilClearValue(float depth, uint32_t stencil);
    ShaderModule loadShaderAsset(VkDevice device, worlds::AssetID id);
} // namespace vku

#undef UNUSED
#endif // VKU_HPP
