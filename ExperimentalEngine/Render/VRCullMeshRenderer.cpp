#include "RenderPasses.hpp"
#include "Render.hpp"
#include <openvr.h>
#include "ShaderCache.hpp"

namespace worlds {
    struct VertPushConstants {
        uint32_t viewOffset;
    };

    void VRCullMeshRenderer::setup(PassSetupCtx& ctx, vk::RenderPass& rp) {
        vku::DescriptorSetLayoutMaker dslm;
        dslm.buffer(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eVertex, 1);
        dsl = dslm.createUnique(ctx.vkCtx.device);

        vku::PipelineLayoutMaker plm;
        plm.pushConstantRange(vk::ShaderStageFlagBits::eVertex, 0, sizeof(VertPushConstants));
        plm.descriptorSetLayout(*dsl);
        pipelineLayout = plm.createUnique(ctx.vkCtx.device);
        
        vku::PipelineMaker pm{ ctx.passWidth, ctx.passHeight };
        pm.cullMode(vk::CullModeFlagBits::eNone);

        auto frag = ShaderCache::getModule(ctx.vkCtx.device, g_assetDB.addOrGetExisting("Shaders/blank.frag.spv"));
        pm.shader(vk::ShaderStageFlagBits::eFragment, frag);

        auto vert = ShaderCache::getModule(ctx.vkCtx.device, g_assetDB.addOrGetExisting("Shaders/vr_hidden.vert.spv"));
        pm.shader(vk::ShaderStageFlagBits::eVertex, vert);

        pm.depthWriteEnable(true).depthTestEnable(true).depthCompareOp(vk::CompareOp::eAlways).polygonMode(vk::PolygonMode::eFill).topology(vk::PrimitiveTopology::eTriangleList);

        vk::PipelineMultisampleStateCreateInfo pmsci;
        pmsci.rasterizationSamples = (vk::SampleCountFlagBits)ctx.vkCtx.graphicsSettings.msaaLevel;
        pm.multisampleState(pmsci);
        pm.subPass(0);
        pm.blendBegin(false);

        pipeline = pm.createUnique(ctx.vkCtx.device, ctx.vkCtx.pipelineCache, *pipelineLayout, rp);

        auto hiddenL = vr::VRSystem()->GetHiddenAreaMesh(vr::EVREye::Eye_Left);
        auto hiddenR = vr::VRSystem()->GetHiddenAreaMesh(vr::EVREye::Eye_Right);

        size_t totalSize = (hiddenL.unTriangleCount + hiddenR.unTriangleCount) * 2 * 3 * sizeof(float);

        float* combinedBuf = (float*)std::malloc(totalSize);
        memcpy(combinedBuf, hiddenL.pVertexData, hiddenL.unTriangleCount * 2 * 3 * sizeof(float));
        memcpy(combinedBuf + hiddenL.unTriangleCount * 2 * 3, hiddenR.pVertexData, hiddenR.unTriangleCount * 2 * 3 * sizeof(float));

        vertexBuf = vku::GenericBuffer{ctx.vkCtx.device, ctx.vkCtx.allocator, 
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, 
            totalSize, VMA_MEMORY_USAGE_GPU_ONLY, "VR Hidden Area Mesh VB"};

        auto q = ctx.vkCtx.device.getQueue(ctx.vkCtx.graphicsQueueFamilyIdx, 0);
        vertexBuf.upload(ctx.vkCtx.device, ctx.vkCtx.commandPool, q, combinedBuf, totalSize);

        vku::DescriptorSetMaker dsm;
        dsm.layout(*dsl);
        ds = dsm.create(ctx.vkCtx.device, ctx.vkCtx.descriptorPool)[0];

        vku::DescriptorSetUpdater dsu;
        dsu.beginDescriptorSet(ds);
        dsu.beginBuffers(0, 0, vk::DescriptorType::eStorageBuffer);
        dsu.buffer(vertexBuf.buffer(), 0, totalSize);
        dsu.update(ctx.vkCtx.device);

        totalVertCount = (hiddenL.unTriangleCount + hiddenR.unTriangleCount) * 3;
        leftVertCount = hiddenL.unTriangleCount * 3;
    }

    void VRCullMeshRenderer::draw(vk::CommandBuffer& cmdBuf) {
        VertPushConstants vpc;
        vpc.viewOffset = leftVertCount;
        cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
        cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, ds, nullptr);
        cmdBuf.pushConstants<VertPushConstants>(*pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, vpc);
        cmdBuf.draw(leftVertCount, 1, 0, 0);
    }
}