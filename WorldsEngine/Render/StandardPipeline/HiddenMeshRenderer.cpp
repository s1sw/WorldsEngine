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
    HiddenMeshRenderer::HiddenMeshRenderer(const EngineInterfaces& interfaces, int sampleCount)
        : interfaces(interfaces)
        , totalVertexCount(0)
    {
        VKRenderer* renderer = (VKRenderer*)interfaces.renderer;
        VK::Core* core = renderer->getCore();

        HiddenMeshData leftMesh{};
        HiddenMeshData rightMesh{};

        //if (!interfaces.vrInterface->getHiddenMeshData(Eye::LeftEye, leftMesh) ||
        //    !interfaces.vrInterface->getHiddenMeshData(Eye::RightEye, rightMesh))
        //{
        //    logWarn(WELogCategoryRender, "VR hidden area mesh was empty");
        //    return;
        //}
        // TODO
        return;

        uint64_t triangleSize = 2 * 3 * sizeof(float);
        uint64_t leftSize = leftMesh.triangleCount * triangleSize;
        uint64_t rightSize = rightMesh.triangleCount * triangleSize;

        if (leftSize != rightSize)
        {
            logErr(WELogCategoryRender, "VR hidden area meshes had different triangle counts for each eye");
            return;
        }

        totalVertexCount = (leftMesh.triangleCount + rightMesh.triangleCount) * 3;
        viewOffset = leftMesh.triangleCount * 3;

        uint64_t totalSize = leftSize + rightSize;
        VK::BufferCreateInfo bci{ VK::BufferUsage::Storage, totalSize };
        vertBuffer = core->CreateBuffer(bci);

        core->QueueBufferUpload(vertBuffer.Get(), leftMesh.verts.data(), leftSize, 0);
        core->QueueBufferUpload(vertBuffer.Get(), rightMesh.verts.data(), rightSize, leftSize);

        VK::DescriptorSetLayoutBuilder dslb{core};
        dslb.Binding(0, VK::DescriptorType::StorageBuffer, 1, VK::ShaderStage::Vertex);
        dsl = dslb.Build();

        VK::PipelineLayoutBuilder plb{core};
        plb.DescriptorSet(dsl.Get());
        plb.PushConstants(VK::ShaderStage::Vertex, 0, sizeof(uint32_t));
        pipelineLayout = plb.Build();

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
            .MSAASamples(sampleCount)
            .ViewMask(0b11)
            .CullMode(VK::CullMode::None);

        pipeline = pb.Build();

        ds = core->CreateDescriptorSet(dsl.Get());

        VK::DescriptorSetUpdater dsu{core, ds.Get()};
        dsu.AddBuffer(0, 0, VK::DescriptorType::StorageBuffer, vertBuffer.Get());
        dsu.Update();
    }

    void HiddenMeshRenderer::Execute(VK::CommandBuffer& cb)
    {
        if (totalVertexCount == 0) return;

        cb.BindPipeline(pipeline.Get());
        cb.BindGraphicsDescriptorSet(pipelineLayout.Get(), ds.Get(), 0);
        cb.PushConstants(viewOffset, VK::ShaderStage::Vertex, pipelineLayout.Get());
        cb.Draw(viewOffset, 1, 0, 0);
    }
}