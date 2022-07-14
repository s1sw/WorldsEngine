#include <R2/VKPipeline.hpp>
#include <R2/VKCore.hpp>
#include <R2/VKDescriptorSet.hpp>
#include <R2/VKTexture.hpp>
#include <volk.h>

namespace R2::VK
{
    ShaderModule::ShaderModule(const Handles* handles, const uint32_t* data, size_t dataLength)
        : handles(handles)
    {
        VkShaderModuleCreateInfo smci{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
        smci.codeSize = dataLength;
        smci.pCode = data;
        VKCHECK(vkCreateShaderModule(handles->Device, &smci, handles->AllocCallbacks, &mod));
    }

    ShaderModule::~ShaderModule()
    {
        vkDestroyShaderModule(handles->Device, mod, handles->AllocCallbacks);
    }

    VkShaderModule ShaderModule::GetNativeHandle()
    {
        return mod;
    }

    PipelineLayout::PipelineLayout(const Handles* handles, VkPipelineLayout layout)
        : layout(layout)
    {}

    PipelineLayout::~PipelineLayout()
    {
        vkDestroyPipelineLayout(handles->Device, layout, handles->AllocCallbacks);
    }

    VkPipelineLayout PipelineLayout::GetNativeHandle()
    {
        return layout;
    }

    PipelineLayoutBuilder::PipelineLayoutBuilder(const Handles* handles)
        : handles(handles)
    {}

    PipelineLayoutBuilder& PipelineLayoutBuilder::PushConstants(ShaderStage stages, uint32_t offset, uint32_t size)
    {
        pushConstants.push_back(PushConstantRange{ stages, offset, size });

        return *this;
    }

    PipelineLayoutBuilder& PipelineLayoutBuilder::DescriptorSet(DescriptorSetLayout* dsl)
    {
        descriptorSetLayouts.push_back(dsl->GetNativeHandle());

        return *this;
    }

    PipelineLayout* PipelineLayoutBuilder::Build()
    {
        static_assert(sizeof(VkPushConstantRange) == sizeof(PushConstantRange));
        VkPipelineLayoutCreateInfo plci{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        plci.pPushConstantRanges = reinterpret_cast<VkPushConstantRange*>(pushConstants.data());
        plci.pushConstantRangeCount = (uint32_t)pushConstants.size();
        plci.setLayoutCount = (uint32_t)descriptorSetLayouts.size();
        plci.pSetLayouts = descriptorSetLayouts.data();
        
        VkPipelineLayout pipelineLayout;
        VKCHECK(vkCreatePipelineLayout(handles->Device, &plci, handles->AllocCallbacks, &pipelineLayout));

        return new PipelineLayout(handles, pipelineLayout);
    }

    Pipeline::Pipeline(const Handles* handles, VkPipeline pipeline)
        : handles(handles)
        , pipeline(pipeline)
    {}

    Pipeline::~Pipeline()
    {
        vkDestroyPipeline(handles->Device, pipeline, handles->AllocCallbacks);
    }

    VkPipeline Pipeline::GetNativeHandle()
    {
        return pipeline;
    }

    PipelineBuilder::PipelineBuilder(const Handles* handles)
        : handles(handles)
    {
        depthFormat = TextureFormat::UNDEFINED;
    }

    VkShaderStageFlagBits convertShaderStage(ShaderStage stage)
    {
        switch (stage)
        {
        case ShaderStage::Fragment:
            return VK_SHADER_STAGE_FRAGMENT_BIT;
        case ShaderStage::Vertex:
        default:
            return VK_SHADER_STAGE_VERTEX_BIT;
        }
    }

    PipelineBuilder& PipelineBuilder::AddShader(ShaderStage stage, ShaderModule& mod)
    {
        shaderStages.emplace_back(mod, stage);
        return *this;
    }

    PipelineBuilder& PipelineBuilder::ColorAttachmentFormat(TextureFormat format)
    {
        attachmentFormats.push_back(format);
        return *this;
    }

    PipelineBuilder& PipelineBuilder::DepthAttachmentFormat(TextureFormat format)
    {
        depthFormat = format;
        return *this;
    }

    PipelineBuilder& PipelineBuilder::AddVertexBinding(VertexBinding&& binding)
    {
        vertexBindings.push_back(binding);
        return *this;
    }

    PipelineBuilder& PipelineBuilder::PrimitiveTopology(Topology topology)
    {
        this->topology = topology;
        return *this;
    }

    PipelineBuilder& PipelineBuilder::CullMode(VK::CullMode cm)
    {
        cullMode = cm;
        return *this;
    }

    PipelineBuilder& PipelineBuilder::Layout(PipelineLayout* layout)
    {
        this->layout = layout->GetNativeHandle();
        return *this;
    }

    PipelineBuilder& PipelineBuilder::AlphaBlend(bool blend)
    {
        this->alphaBlend = blend;
        return *this;
    }

    PipelineBuilder& PipelineBuilder::DepthTest(bool enable)
    {
        depthTest = enable;
        return *this;
    }

    PipelineBuilder& PipelineBuilder::DepthWrite(bool enable)
    {
        depthWrite = enable;
        return *this;
    }

    PipelineBuilder& PipelineBuilder::DepthCompareOp(CompareOp op)
    {
        depthCompareOp = op;
        return *this;
    }

    Pipeline* PipelineBuilder::Build()
    {
        // Rendering state
        VkPipelineRenderingCreateInfo renderingCI{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
        renderingCI.colorAttachmentCount = (uint32_t)attachmentFormats.size();
        renderingCI.pColorAttachmentFormats = reinterpret_cast<VkFormat*>(&attachmentFormats[0]);
        renderingCI.depthAttachmentFormat = static_cast<VkFormat>(depthFormat);

        // Convert vertex bindings
        std::vector<VkVertexInputBindingDescription> bindingDescs;
        std::vector<VkVertexInputAttributeDescription> attributeDescs;

        for (const VertexBinding& vb : vertexBindings)
        {
            VkVertexInputBindingDescription desc{};
            desc.binding = vb.Binding;
            desc.stride = vb.Size;
            desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            for (const VertexAttribute& va : vb.Attributes)
            {
                VkVertexInputAttributeDescription adesc{};
                adesc.binding = vb.Binding;
                adesc.location = va.Index;
                adesc.offset = va.Offset;
                adesc.format = static_cast<VkFormat>(va.Format);

                attributeDescs.push_back(adesc);
            }

            bindingDescs.push_back(desc);
        }

        // Vertex input state
        VkPipelineVertexInputStateCreateInfo vertexInputStateCI{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
        vertexInputStateCI.pVertexBindingDescriptions = bindingDescs.data();
        vertexInputStateCI.vertexBindingDescriptionCount = (uint32_t)bindingDescs.size();

        vertexInputStateCI.pVertexAttributeDescriptions = attributeDescs.data();
        vertexInputStateCI.vertexAttributeDescriptionCount = (uint32_t)attributeDescs.size();

        // Input assembly state
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
        inputAssemblyStateCI.topology = static_cast<VkPrimitiveTopology>(topology);

        // Dynamic state
        VkPipelineDynamicStateCreateInfo dynamicStateCI{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
        VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        dynamicStateCI.pDynamicStates = dynamicStates;
        dynamicStateCI.dynamicStateCount = 2;

        // Rasterization state
        VkPipelineRasterizationStateCreateInfo rasterizationStateCI{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
        rasterizationStateCI.cullMode = static_cast<VkCullModeFlagBits>(cullMode);
        rasterizationStateCI.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizationStateCI.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizationStateCI.lineWidth = 1.0f;

        // Depth stencil state
        VkPipelineDepthStencilStateCreateInfo depthStencilStateCI{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
        depthStencilStateCI.depthTestEnable = depthTest;
        depthStencilStateCI.depthWriteEnable = depthWrite;
        depthStencilStateCI.depthCompareOp = static_cast<VkCompareOp>(depthCompareOp);

        // Multisample state
        VkPipelineMultisampleStateCreateInfo multisampleStateCI{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
        multisampleStateCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // Attachment blend states
        std::vector<VkPipelineColorBlendAttachmentState> attachmentBlendStates;

        for (size_t i = 0; i < attachmentFormats.size(); i++)
        {
            VkPipelineColorBlendAttachmentState cbas{};
            cbas.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

            if (!alphaBlend)
            {
                cbas.blendEnable = VK_FALSE;
            }
            else
            {
                cbas.blendEnable = VK_TRUE;
                cbas.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                cbas.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                cbas.colorBlendOp = VK_BLEND_OP_ADD;
                cbas.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                cbas.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                cbas.alphaBlendOp = VK_BLEND_OP_ADD;
            }
            attachmentBlendStates.push_back(cbas);
        }

        // Blend info
        VkPipelineColorBlendStateCreateInfo colorBlendStateCI{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        colorBlendStateCI.attachmentCount = (uint32_t)attachmentFormats.size();
        colorBlendStateCI.pAttachments = attachmentBlendStates.data();
        colorBlendStateCI.logicOpEnable = VK_FALSE;

        // Viewport state
        VkPipelineViewportStateCreateInfo viewportStateCI{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };

        // Since we use dynamic viewport, there isn't much meaning to what we set here
        VkRect2D scissorRect{ 0, 0, 1280, 720 };
        VkViewport viewport{ 0.0f, 0.0f, 1280.0f, 720.0f };
        viewportStateCI.pScissors = &scissorRect;
        viewportStateCI.scissorCount = 1;
        viewportStateCI.pViewports = &viewport;
        viewportStateCI.viewportCount = 1;

        std::vector<VkPipelineShaderStageCreateInfo> vkShaderStages;
        vkShaderStages.reserve(shaderStages.size());

        for (const ShaderStageCreateInfo& stage : shaderStages) {
            VkPipelineShaderStageCreateInfo vkStage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            vkStage.stage = convertShaderStage(stage.stage);
            vkStage.module = stage.module.GetNativeHandle();
            vkStage.pName = "main";
            vkShaderStages.push_back(vkStage);
        }

        VkGraphicsPipelineCreateInfo pci{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        pci.renderPass = VK_NULL_HANDLE;
        pci.pStages = vkShaderStages.data();
        pci.stageCount = (uint32_t)vkShaderStages.size();
        pci.pVertexInputState = &vertexInputStateCI;
        pci.pInputAssemblyState = &inputAssemblyStateCI;
        pci.pRasterizationState = &rasterizationStateCI;
        pci.pDepthStencilState = &depthStencilStateCI;
        pci.pColorBlendState = &colorBlendStateCI;
        pci.pDynamicState = &dynamicStateCI;
        pci.pMultisampleState = &multisampleStateCI;
        pci.pViewportState = &viewportStateCI;
        pci.pNext = &renderingCI;
        pci.layout = layout;

        VkPipeline pipeline;
        VKCHECK(vkCreateGraphicsPipelines(handles->Device, nullptr, 1, &pci, handles->AllocCallbacks, &pipeline));

        return new Pipeline(handles, pipeline);
    }
}