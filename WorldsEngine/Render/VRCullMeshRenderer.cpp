#include "RenderPasses.hpp"
#include "Render.hpp"
#include <openvr.h>
#include "ShaderCache.hpp"

namespace worlds {
    struct VertPushConstants {
        uint32_t viewOffset;
    };

    VRCullMeshRenderer::VRCullMeshRenderer(VulkanHandles* handles)
        : handles {handles} {
    }

    void VRCullMeshRenderer::setup(RenderContext& ctx, vk::RenderPass& rp, vk::DescriptorPool descriptorPool) {
        vku::DescriptorSetLayoutMaker dslm;
        dslm.buffer(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eVertex, 1);
        dsl = dslm.createUnique(handles->device);

        vku::PipelineLayoutMaker plm;
        plm.pushConstantRange(vk::ShaderStageFlagBits::eVertex, 0, sizeof(VertPushConstants));
        plm.descriptorSetLayout(*dsl);
        pipelineLayout = plm.createUnique(handles->device);

        vku::PipelineMaker pm{ ctx.passWidth, ctx.passHeight };
        pm.cullMode(vk::CullModeFlagBits::eNone);

        auto frag = ShaderCache::getModule(handles->device, AssetDB::pathToId("Shaders/blank.frag.spv"));
        pm.shader(vk::ShaderStageFlagBits::eFragment, frag);

        auto vert = ShaderCache::getModule(handles->device, AssetDB::pathToId("Shaders/vr_hidden.vert.spv"));
        pm.shader(vk::ShaderStageFlagBits::eVertex, vert);

        pm.depthWriteEnable(true).depthTestEnable(true).depthCompareOp(vk::CompareOp::eAlways).polygonMode(vk::PolygonMode::eFill).topology(vk::PrimitiveTopology::eTriangleList);

        vk::PipelineMultisampleStateCreateInfo pmsci;
        pmsci.rasterizationSamples = (vk::SampleCountFlagBits)handles->graphicsSettings.msaaLevel;
        pm.multisampleState(pmsci);
        pm.subPass(0);
        pm.blendBegin(false);

        pipeline = pm.createUnique(handles->device, handles->pipelineCache, *pipelineLayout, rp);

        auto hiddenL = vr::VRSystem()->GetHiddenAreaMesh(vr::EVREye::Eye_Left);
        auto hiddenR = vr::VRSystem()->GetHiddenAreaMesh(vr::EVREye::Eye_Right);

        size_t totalSize = (hiddenL.unTriangleCount + hiddenR.unTriangleCount) * 2 * 3 * sizeof(float);

        float* combinedBuf = (float*)std::malloc(totalSize);
        memcpy(combinedBuf, hiddenL.pVertexData, hiddenL.unTriangleCount * 2 * 3 * sizeof(float));
        memcpy(combinedBuf + hiddenL.unTriangleCount * 2 * 3, hiddenR.pVertexData, hiddenR.unTriangleCount * 2 * 3 * sizeof(float));

        vertexBuf = vku::GenericBuffer{handles->device, handles->allocator,
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
            totalSize, VMA_MEMORY_USAGE_GPU_ONLY, "VR Hidden Area Mesh VB"};

        auto q = handles->device.getQueue(handles->graphicsQueueFamilyIdx, 0);
        vertexBuf.upload(handles->device, handles->commandPool, q, combinedBuf, totalSize);

        vku::DescriptorSetMaker dsm;
        dsm.layout(*dsl);
        ds = std::move(dsm.createUnique(handles->device, descriptorPool)[0]);

        vku::DescriptorSetUpdater dsu;
        dsu.beginDescriptorSet(*ds);
        dsu.beginBuffers(0, 0, vk::DescriptorType::eStorageBuffer);
        dsu.buffer(vertexBuf.buffer(), 0, totalSize);
        dsu.update(handles->device);

        totalVertCount = (hiddenL.unTriangleCount + hiddenR.unTriangleCount) * 3;
        leftVertCount = hiddenL.unTriangleCount * 3;
    }

    void VRCullMeshRenderer::draw(vk::CommandBuffer& cmdBuf) {
        VertPushConstants vpc;
        vpc.viewOffset = leftVertCount;
        cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
        cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, *ds, nullptr);
        cmdBuf.pushConstants<VertPushConstants>(*pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, vpc);
        cmdBuf.draw(leftVertCount, 1, 0, 0);
    }
}
