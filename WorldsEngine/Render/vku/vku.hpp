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

    /// Factory for instances.
    class InstanceMaker {
    public:
        InstanceMaker();

        /// Set the default layers and extensions.
        InstanceMaker& defaultLayers();

        /// Add a layer. eg. "VK_LAYER_LUNARG_standard_validation"
        InstanceMaker& layer(const char* layer);

        /// Add an extension. eg. VK_EXT_DEBUG_REPORT_EXTENSION_NAME
        InstanceMaker& extension(const char* layer);

        /// Set the name of the application.
        InstanceMaker& applicationName(const char* pApplicationName_);

        /// Set the version of the application.
        InstanceMaker& applicationVersion(uint32_t applicationVersion_);

        /// Set the name of the engine.
        InstanceMaker& engineName(const char* pEngineName_);

        /// Set the version of the engine.
        InstanceMaker& engineVersion(uint32_t engineVersion_);

        /// Set the version of the api.
        InstanceMaker& apiVersion(uint32_t apiVersion_);

        /// Create an instance.
        VkInstance create();
    private:
        std::vector<const char*> layers_;
        std::vector<const char*> instance_extensions_;
        VkApplicationInfo app_info_{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
    };

    /// Factory for devices.
    class DeviceMaker {
    public:
        /// Make queues and a logical device for a certain physical device.
        DeviceMaker();

        /// Set the default layers and extensions.
        DeviceMaker& defaultLayers();

        /// Add a layer. eg. "VK_LAYER_LUNARG_standard_validation"
        DeviceMaker& layer(const char* layer);

        /// Add an extension. eg. VK_EXT_DEBUG_REPORT_EXTENSION_NAME
        DeviceMaker& extension(const char* layer);

        /// Add one or more queues to the device from a certain family.
        DeviceMaker& queue(uint32_t familyIndex, float priority = 0.0f, uint32_t n = 1);

        DeviceMaker& setPNext(void* next);

        DeviceMaker& setFeatures(VkPhysicalDeviceFeatures& features);

        /// Create a new logical device.
        VkDevice create(VkPhysicalDevice physical_device);
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
        DebugCallback();

        DebugCallback(
            VkInstance instance,
            VkDebugReportFlagsEXT flags =
            VK_DEBUG_REPORT_WARNING_BIT_EXT |
            VK_DEBUG_REPORT_ERROR_BIT_EXT
        );

        void reset();
    private:
        // Report any errors or warnings.
        static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
            VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType,
            uint64_t object, size_t location, int32_t messageCode,
            const char* pLayerPrefix, const char* pMessage, void* pUserData);

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
