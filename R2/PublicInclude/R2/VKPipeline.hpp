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
    class Core;

    struct VertexAttribute
    {
        // Declare an explicit constructor otherwise Clang complains about
        // emplace_back
        VertexAttribute(int index, TextureFormat format, uint32_t offset)
            : Index(index)
            , Format(format)
            , Offset(offset) {}

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

    class PipelineLayout
    {
    public:
        PipelineLayout(const Handles* handles, VkPipelineLayout layout);
        ~PipelineLayout();
        VkPipelineLayout GetNativeHandle();
    private:
        const Handles* handles;
        VkPipelineLayout layout;
    };

    class PipelineLayoutBuilder
    {
    public:
        PipelineLayoutBuilder(const Handles* handles);
        PipelineLayoutBuilder(Core* core);
        PipelineLayoutBuilder& PushConstants(ShaderStage stages, uint32_t offset, uint32_t size);
        PipelineLayoutBuilder& DescriptorSet(DescriptorSetLayout* layout);
        PipelineLayout* Build();
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
        Pipeline(Core* core, VkPipeline pipeline);
        ~Pipeline();
        VkPipeline GetNativeHandle();
    private:
        Core* core;
        VkPipeline pipeline;
    };

    class PipelineBuilder
    {
    public:
        PipelineBuilder(Core* core);
        PipelineBuilder& AddShader(ShaderStage stage, ShaderModule& mod);
        PipelineBuilder& ColorAttachmentFormat(TextureFormat format);
        PipelineBuilder& DepthAttachmentFormat(TextureFormat format);
        PipelineBuilder& AddVertexBinding(VertexBinding&& Binding);
        PipelineBuilder& PrimitiveTopology(Topology topology);
        PipelineBuilder& CullMode(CullMode mode);
        PipelineBuilder& Layout(PipelineLayout* layout);
        PipelineBuilder& AlphaBlend(bool blend);
        PipelineBuilder& DepthTest(bool enable);
        PipelineBuilder& DepthWrite(bool enable);
        PipelineBuilder& DepthCompareOp(CompareOp op);
        PipelineBuilder& MSAASamples(int sampleCount);
        PipelineBuilder& ViewMask(uint32_t viewMask);
        PipelineBuilder& DepthBias(bool enable);
        PipelineBuilder& ConstantDepthBias(float b);
        PipelineBuilder& SlopeDepthBias(float b);
        Pipeline* Build();
    private:
        Core* core;

        struct ShaderStageCreateInfo {
            ShaderModule& module;
            ShaderStage stage;
        };

        std::vector<TextureFormat> attachmentFormats;
        TextureFormat depthFormat;
        std::vector<ShaderStageCreateInfo> shaderStages;
        std::vector<VertexBinding> vertexBindings;
        Topology topology = Topology::TriangleList;
        VK::CullMode cullMode = VK::CullMode::Back;
        VkPipelineLayout layout;
        bool alphaBlend = false;
        bool depthTest = false;
        bool depthWrite = false;
        bool depthBias = false;
        float constantDepthBias = 0.0f;
        float slopeDepthBias = 0.0f;
        CompareOp depthCompareOp = CompareOp::Always;
        int numSamples = 1;
        uint32_t viewMask = 0;
    };

    class ComputePipelineBuilder
    {
    public:
        ComputePipelineBuilder(Core* core);
        ComputePipelineBuilder& SetShader(ShaderModule& mod);
        ComputePipelineBuilder& Layout(PipelineLayout* layout);
        Pipeline* Build();
    private:
        Core* core;
        ShaderModule* shaderModule;
        VkPipelineLayout pipelineLayout;
    };
}