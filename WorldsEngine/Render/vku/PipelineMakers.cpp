#include "vku.hpp"

namespace vku {
    PipelineLayoutMaker::PipelineLayoutMaker() {}

    VkPipelineLayout PipelineLayoutMaker::create(VkDevice device) const {
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            nullptr, {}, (uint32_t)setLayouts_.size(),
            setLayouts_.data(), (uint32_t)pushConstantRanges_.size(),
            pushConstantRanges_.data() };

        VkPipelineLayout layout;
        VKCHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &layout));

        return layout;
    }

    void PipelineLayoutMaker::descriptorSetLayout(VkDescriptorSetLayout layout) {
        setLayouts_.push_back(layout);
    }

    void PipelineLayoutMaker::pushConstantRange(VkShaderStageFlags stageFlags_, uint32_t offset_, uint32_t size_) {
        pushConstantRanges_.emplace_back(stageFlags_, offset_, size_);
    }

    PipelineMaker::PipelineMaker(uint32_t width, uint32_t height) {
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

    VkPipeline PipelineMaker::create(VkDevice device,
        const VkPipelineCache& pipelineCache,
        const VkPipelineLayout& pipelineLayout,
        const VkRenderPass& renderPass, bool defaultBlend) {

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

    void PipelineMaker::shader(VkShaderStageFlagBits stage, vku::ShaderModule& shader,
        const char* entryPoint, VkSpecializationInfo* pSpecializationInfo) {
        VkPipelineShaderStageCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        info.module = shader.module();
        info.pName = entryPoint;
        info.stage = stage;
        info.pSpecializationInfo = pSpecializationInfo;
        modules_.emplace_back(info);
    }

    void PipelineMaker::shader(VkShaderStageFlagBits stage, VkShaderModule shader,
        const char* entryPoint, VkSpecializationInfo* pSpecializationInfo) {
        VkPipelineShaderStageCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        info.module = shader;
        info.pName = entryPoint;
        info.stage = stage;
        info.pSpecializationInfo = pSpecializationInfo;
        modules_.emplace_back(info);
    }

    void PipelineMaker::blendBegin(VkBool32 enable) {
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

    ComputePipelineMaker::ComputePipelineMaker() {}

    VkPipeline ComputePipelineMaker::create(VkDevice device, const VkPipelineCache& pipelineCache, const VkPipelineLayout& pipelineLayout) {
        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;

        pipelineInfo.stage = stage_;
        pipelineInfo.layout = pipelineLayout;

        VkPipeline pipeline;
        VKCHECK(vkCreateComputePipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &pipeline));

        return pipeline;
    }

    void ComputePipelineMaker::shader(VkShaderStageFlagBits stage, vku::ShaderModule& shader, const char* entryPoint) {
        stage_.module = shader.module();
        stage_.pName = entryPoint;
        stage_.stage = stage;
    }

    void ComputePipelineMaker::shader(VkShaderStageFlagBits stage, VkShaderModule shader, const char* entryPoint) {
        stage_.module = shader;
        stage_.pName = entryPoint;
        stage_.stage = stage;
    }
}
