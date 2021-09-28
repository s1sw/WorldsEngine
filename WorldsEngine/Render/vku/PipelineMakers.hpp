#pragma once
#include "vku.hpp"

namespace vku {
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
        ComputePipelineMaker();

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
        VkPipeline create(VkDevice device, const VkPipelineCache& pipelineCache, const VkPipelineLayout& pipelineLayout);
    private:
        VkPipelineShaderStageCreateInfo stage_{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        VkSpecializationInfo info_;
    };
}
