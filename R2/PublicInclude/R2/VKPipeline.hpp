#pragma once
#include <stdint.h>
#include <vector>
#include <R2/VKEnums.hpp>

#define VK_DEFINE_HANDLE(object) typedef struct object##_T* object;
VK_DEFINE_HANDLE(VkPipeline)
VK_DEFINE_HANDLE(VkShaderModule)
VK_DEFINE_HANDLE(VkPipelineLayout)
VK_DEFINE_HANDLE(VkDescriptorSetLayout)
#undef VK_DEFINE_HANDLE

struct VkPipelineShaderStageCreateInfo;

namespace R2::VK
{
    struct Handles;
    enum class TextureFormat;
    class DescriptorSetLayout;

    struct VertexAttribute
    {
        int Index;
        TextureFormat Format;
        uint32_t Offset;
    };

    struct VertexBinding
    {
        int Binding;
        uint32_t Size;

        std::vector<VertexAttribute> Attributes;
    };

    enum class Topology
    {
        PointList = 0,
        LineList = 1,
        LineStrip = 2,
        TriangleList = 3,
        TriangleStrip = 4,
        TriangleFan = 5
    };

    enum class CullMode
    {
        None = 0,
        Front = 1,
        Back = 2,
        FrontAndBack = 3
    };

    class ShaderModule
    {
    public:
        ShaderModule(const Handles* handles, const uint32_t* data, size_t dataLength);
        ~ShaderModule();
        VkShaderModule GetNativeHandle();
    private:
        VkShaderModule mod;
        const Handles* handles;
    };

    class PipelineLayoutBuilder
    {
    public:
        PipelineLayoutBuilder(const Handles* handles);
        PipelineLayoutBuilder& PushConstants(ShaderStage stages, uint32_t offset, uint32_t size);
        PipelineLayoutBuilder& DescriptorSet(DescriptorSetLayout* layout);
        VkPipelineLayout Build();
    private:
        struct PushConstantRange
        {
            ShaderStage Stages;
            uint32_t Offset;
            uint32_t Size;
        };

        const Handles* handles;
        std::vector<PushConstantRange> pushConstants;
        std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    };

    class Pipeline
    {
    public:
        Pipeline(const Handles* handles, VkPipeline pipeline);
        ~Pipeline();
        VkPipeline GetNativeHandle();
    private:
        const Handles* handles;
        VkPipeline pipeline;
    };

    class PipelineBuilder
    {
    public:
        PipelineBuilder(const Handles* handles);
        PipelineBuilder& AddShader(ShaderStage stage, ShaderModule& mod);
        PipelineBuilder& ColorAttachmentFormat(TextureFormat format);
        PipelineBuilder& AddVertexBinding(VertexBinding&& Binding);
        PipelineBuilder& PrimitiveTopology(Topology topology);
        PipelineBuilder& CullMode(CullMode mode);
        PipelineBuilder& Layout(VkPipelineLayout layout);
        PipelineBuilder& AlphaBlend(bool blend);
        Pipeline* Build();
    private:
        const Handles* handles;

        std::vector<TextureFormat> attachmentFormats;
        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
        std::vector<VertexBinding> vertexBindings;
        Topology topology;
        VK::CullMode cullMode;
        VkPipelineLayout layout;
        bool alphaBlend = false;
    };
}