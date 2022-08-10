#include <Render/StandardPipeline/DebugLineDrawer.hpp>
#include <Core/AssetDB.hpp>
#include <R2/VK.hpp>
#include <Render/ShaderCache.hpp>
#include <Render/RenderInternal.hpp>

using namespace R2;

namespace worlds
{
    struct DebugLineVert
    {
        glm::vec3 point;
        glm::vec4 color;
    };

    DebugLineDrawer::DebugLineDrawer(VK::Core* core, VK::Buffer* vpBuffer, int msaaLevel)
        : core(core)
    {
        VK::DescriptorSetLayoutBuilder dslb{core->GetHandles()};
        dslb.Binding(0, VK::DescriptorType::UniformBuffer, 1, VK::ShaderStage::AllRaster);
        dsl = dslb.Build();
        ds = core->CreateDescriptorSet(dsl.Get());

        VK::DescriptorSetUpdater dsu{core->GetHandles(), ds.Get()};
        dsu.AddBuffer(0, 0, VK::DescriptorType::UniformBuffer, vpBuffer);
        dsu.Update();

        VK::PipelineLayoutBuilder plb{core->GetHandles()};
        plb.DescriptorSet(dsl.Get());
        pipelineLayout = plb.Build();

        VK::VertexBinding vb;
        vb.Size = sizeof(DebugLineVert);
        vb.Binding = 0;
        vb.Attributes.emplace_back(0, VK::TextureFormat::R32G32B32_SFLOAT, offsetof(DebugLineVert, point));
        vb.Attributes.emplace_back(1, VK::TextureFormat::R32G32B32A32_SFLOAT, offsetof(DebugLineVert, color));

        VK::ShaderModule& vert = ShaderCache::getModule(AssetDB::pathToId("Shaders/debug_line.vert.spv"));
        VK::ShaderModule& frag = ShaderCache::getModule(AssetDB::pathToId("Shaders/debug_line.frag.spv"));

        VK::PipelineBuilder pb{core};
        pb.PrimitiveTopology(VK::Topology::LineList)
            .Layout(pipelineLayout.Get())
            .ColorAttachmentFormat(VK::TextureFormat::B10G11R11_UFLOAT_PACK32)
            .CullMode(VK::CullMode::None)
            .AddVertexBinding(std::move(vb))
            .AddShader(VK::ShaderStage::Vertex, vert)
            .AddShader(VK::ShaderStage::Fragment, frag)
            .DepthTest(true)
            .DepthWrite(true)
            .DepthCompareOp(VK::CompareOp::Greater)
            .DepthAttachmentFormat(VK::TextureFormat::D32_SFLOAT)
            .MSAASamples(msaaLevel);

        pipeline = pb.Build();
    }

    void DebugLineDrawer::Execute(VK::CommandBuffer& cb, const DebugLine* debugLines, size_t debugLineCount)
    {
        if (debugLineBuffer.Get() == nullptr || debugLineCount > debugLineBuffer->GetSize() / (sizeof(DebugLineVert) * 2))
        {
            VK::BufferCreateInfo bci{};
            bci.Mappable = true;
            bci.Size = sizeof(DebugLineVert) * (debugLineCount + 100) * 2;
            bci.Usage = VK::BufferUsage::Vertex;
            debugLineBuffer = core->CreateBuffer(bci);
        }

        DebugLineVert* mappedBuffer = (DebugLineVert*)debugLineBuffer->Map();

        for (size_t i = 0; i < debugLineCount; i++)
        {
            const DebugLine& dl = debugLines[i];
            mappedBuffer[i * 2] = DebugLineVert { dl.p0, dl.color };
            mappedBuffer[i * 2 + 1] = DebugLineVert { dl.p1, dl.color };
        }

        debugLineBuffer->Unmap();

        cb.BindVertexBuffer(0, debugLineBuffer.Get(), 0);
        cb.BindGraphicsDescriptorSet(pipelineLayout.Get(), ds->GetNativeHandle(), 0);
        cb.BindPipeline(pipeline.Get());
        cb.Draw(debugLineCount * 2, 1, 0, 0);
    }
}