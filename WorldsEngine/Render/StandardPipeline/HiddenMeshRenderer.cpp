#include "HiddenMeshRenderer.hpp"
#include <Core/Log.hpp>
#include <Render/ShaderCache.hpp>
#include <R2/VK.hpp>
#include <openvr.h>

using namespace R2;

namespace worlds
{
    HiddenMeshRenderer::HiddenMeshRenderer(R2::VK::Core* core, int sampleCount)
        : core(core)
    {
        vr::HiddenAreaMesh_t leftMesh = vr::VRSystem()->GetHiddenAreaMesh(vr::Eye_Left);
        vr::HiddenAreaMesh_t rightMesh = vr::VRSystem()->GetHiddenAreaMesh(vr::Eye_Right);

        if (leftMesh.unTriangleCount == 0 || rightMesh.unTriangleCount == 0)
        {
            logWarn(WELogCategoryRender, "VR hidden area mesh was empty");
            return;
        }

        uint64_t triangleSize = 2 * 3 * sizeof(float);
        uint64_t leftSize = leftMesh.unTriangleCount * triangleSize;
        uint64_t rightSize = rightMesh.unTriangleCount * triangleSize;

        if (leftSize != rightSize)
        {
            logErr(WELogCategoryRender, "VR hidden area meshes had different triangle counts for each eye");
            return;
        }

        totalVertexCount = (leftMesh.unTriangleCount + rightMesh.unTriangleCount) * 3;
        viewOffset = leftMesh.unTriangleCount * 3;

        uint64_t totalSize = leftSize + rightSize;
        VK::BufferCreateInfo bci{ VK::BufferUsage::Storage, totalSize };
        vertBuffer = core->CreateBuffer(bci);

        core->QueueBufferUpload(vertBuffer.Get(), leftMesh.pVertexData, leftSize, 0);
        core->QueueBufferUpload(vertBuffer.Get(), rightMesh.pVertexData, rightSize, leftSize);

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
        cb.BindGraphicsDescriptorSet(pipelineLayout.Get(), ds->GetNativeHandle(), 0);
        cb.PushConstants(viewOffset, VK::ShaderStage::Vertex, pipelineLayout.Get());
        cb.Draw(viewOffset, 1, 0, 0);
    }
}