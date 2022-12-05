#include "HiddenMeshRenderer.hpp"
#include <Core/Engine.hpp>
#include <Core/Log.hpp>
#include <Render/RenderInternal.hpp>
#include <Render/ShaderCache.hpp>
#include <R2/VK.hpp>
#include <VR/OpenXRInterface.hpp>

using namespace R2;

namespace worlds
{
    HiddenMeshRenderer::HiddenMeshRenderer(const EngineInterfaces& interfaces, int sampleCount, VK::Buffer* vpBuffer)
        : interfaces(interfaces)
        , leftIndexCount(0)
        , rightIndexCount(0)
    {
        VKRenderer* renderer = (VKRenderer*)interfaces.renderer;
        VK::Core* core = renderer->getCore();

        HiddenAreaMesh leftMesh{};
        HiddenAreaMesh rightMesh{};

        if (!interfaces.vrInterface->getHiddenAreaMesh(Eye::LeftEye, leftMesh) ||
            !interfaces.vrInterface->getHiddenAreaMesh(Eye::RightEye, rightMesh))
        {
            logWarn(WELogCategoryRender, "VR hidden area mesh was empty");
            return;
        }

        uint64_t totalVerts = leftMesh.verts.size() + rightMesh.verts.size();
        uint64_t totalIndices = leftMesh.indices.size() + rightMesh.indices.size();

        VK::BufferCreateInfo bci{ VK::BufferUsage::Vertex, totalVerts * sizeof(glm::vec2) };
        vertexBuffer = core->CreateBuffer(bci);
        bci.Usage = VK::BufferUsage::Index;
        bci.Size = totalIndices * sizeof(uint32_t);
        indexBuffer = core->CreateBuffer(bci);

        size_t leftVertSize = leftMesh.verts.size() * sizeof(glm::vec2);
        size_t rightVertSize = rightMesh.verts.size() * sizeof(glm::vec2);

        core->QueueBufferUpload(vertexBuffer.Get(), leftMesh.verts.data(), leftVertSize, 0);
        core->QueueBufferUpload(vertexBuffer.Get(), rightMesh.verts.data(), rightVertSize, leftVertSize);

        size_t leftIndicesSize = leftMesh.indices.size() * sizeof(uint32_t);
        size_t rightIndicesSize = rightMesh.indices.size() * sizeof(uint32_t);

        core->QueueBufferUpload(indexBuffer.Get(), leftMesh.indices.data(), leftIndicesSize, 0);
        core->QueueBufferUpload(indexBuffer.Get(), rightMesh.indices.data(), rightIndicesSize, leftIndicesSize);


        VK::DescriptorSetLayoutBuilder dslb{core};
        dslb.Binding(0, VK::DescriptorType::UniformBuffer, 1, VK::ShaderStage::Vertex);
        dsl = dslb.Build();

        ds = core->CreateDescriptorSet(dsl.Get());

        VK::DescriptorSetUpdater dsu{ core, ds.Get() };
        dsu.AddBuffer(0, 0, VK::DescriptorType::UniformBuffer, vpBuffer);
        dsu.Update();

        VK::PipelineLayoutBuilder plb{core};
        plb.PushConstants(VK::ShaderStage::Vertex, 0, sizeof(uint32_t));
        plb.DescriptorSet(dsl.Get());
        pipelineLayout = plb.Build();

        VK::VertexBinding vb{};
        vb.Size = sizeof(glm::vec2);
        vb.Attributes.emplace_back(0, VK::TextureFormat::R32G32_SFLOAT, 0);

        VK::PipelineBuilder pb{core};
        pb
            .Layout(pipelineLayout.Get())
            .AddShader(VK::ShaderStage::Vertex, ShaderCache::getModule("Shaders/vr_hidden.vert.spv"))
            .AddShader(VK::ShaderStage::Fragment, ShaderCache::getModule("Shaders/blank.frag.spv"))
            .DepthAttachmentFormat(VK::TextureFormat::D32_SFLOAT)
            .DepthWrite(true)
            .DepthTest(true)
            .DepthCompareOp(VK::CompareOp::Always)
            .PrimitiveTopology(VK::Topology::TriangleList)
            .AddVertexBinding(vb)
            .MSAASamples(sampleCount)
            .ViewMask(0b11)
            .CullMode(VK::CullMode::None);

        pipeline = pb.Build();

        leftIndexCount = leftMesh.indices.size();
        rightIndexCount = rightMesh.indices.size();
        leftVertCount = leftMesh.verts.size();
    }

    void HiddenMeshRenderer::Execute(VK::CommandBuffer& cb)
    {
        if (leftIndexCount == 0 || rightIndexCount == 0) return;

        cb.BindVertexBuffer(0, vertexBuffer.Get(), 0);
        cb.BindIndexBuffer(indexBuffer.Get(), 0, VK::IndexType::Uint32);

        cb.BindPipeline(pipeline.Get());
        cb.BindGraphicsDescriptorSet(pipelineLayout.Get(), ds.Get(), 0);

        uint32_t view = 0;
        cb.PushConstants(view, VK::ShaderStage::Vertex, pipelineLayout.Get());
        cb.DrawIndexed(leftIndexCount, 1, 0, 0, 0);

        view = 1;
        cb.PushConstants(view, VK::ShaderStage::Vertex, pipelineLayout.Get());
        cb.DrawIndexed(rightIndexCount, 1, leftIndexCount, (int)leftVertCount, 0);
    }
}
