#include "RenderPasses.hpp"
#include "Render.hpp"
#include <openvr.h>
#include "ShaderCache.hpp"
#include "vku/DescriptorSetUtil.hpp"

namespace worlds {
    struct VertPushConstants {
        uint32_t viewOffset;
    };

    VRCullMeshRenderer::VRCullMeshRenderer(VulkanHandles* handles)
        : handles {handles} {
    }

    void VRCullMeshRenderer::setup(RenderContext& ctx, VkRenderPass rp, VkDescriptorPool descriptorPool) {
        vku::DescriptorSetLayoutMaker dslm;
        dslm.buffer(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1);
        dsl = dslm.create(handles->device);

        vku::PipelineLayoutMaker plm;
        plm.pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VertPushConstants));
        plm.descriptorSetLayout(dsl);
        pipelineLayout = plm.create(handles->device);

        vku::PipelineMaker pm{ ctx.passWidth, ctx.passHeight };
        pm.cullMode(VK_CULL_MODE_NONE);

        auto frag = ShaderCache::getModule(handles->device, AssetDB::pathToId("Shaders/blank.frag.spv"));
        pm.shader(VK_SHADER_STAGE_FRAGMENT_BIT, frag);

        auto vert = ShaderCache::getModule(handles->device, AssetDB::pathToId("Shaders/vr_hidden.vert.spv"));
        pm.shader(VK_SHADER_STAGE_VERTEX_BIT, vert);

        pm
            .depthWriteEnable(true)
            .depthTestEnable(true)
            .depthCompareOp(VK_COMPARE_OP_ALWAYS)
            .polygonMode(VK_POLYGON_MODE_FILL)
            .topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

        pm.rasterizationSamples((VkSampleCountFlagBits)handles->graphicsSettings.msaaLevel);
        pm.subPass(0);
        pm.blendBegin(false);
        pm.dynamicState(VK_DYNAMIC_STATE_VIEWPORT);
        pm.dynamicState(VK_DYNAMIC_STATE_SCISSOR);

        pipeline = pm.create(handles->device, handles->pipelineCache, pipelineLayout, rp);

        auto hiddenL = vr::VRSystem()->GetHiddenAreaMesh(vr::EVREye::Eye_Left);
        auto hiddenR = vr::VRSystem()->GetHiddenAreaMesh(vr::EVREye::Eye_Right);

        size_t totalSize = (hiddenL.unTriangleCount + hiddenR.unTriangleCount) * 2 * 3 * sizeof(float);

        float* combinedBuf = (float*)std::malloc(totalSize);
        memcpy(combinedBuf, hiddenL.pVertexData, hiddenL.unTriangleCount * 2 * 3 * sizeof(float));
        memcpy(combinedBuf + hiddenL.unTriangleCount * 2 * 3, hiddenR.pVertexData, hiddenR.unTriangleCount * 2 * 3 * sizeof(float));

        vertexBuf = vku::GenericBuffer{handles->device, handles->allocator,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            totalSize, VMA_MEMORY_USAGE_GPU_ONLY, "VR Hidden Area Mesh VB"};

        VkQueue queue;
        vkGetDeviceQueue(handles->device, handles->graphicsQueueFamilyIdx, 0, &queue);
        vertexBuf.upload(handles->device, handles->commandPool, queue, combinedBuf, totalSize);

        vku::DescriptorSetMaker dsm;
        dsm.layout(dsl);
        ds = dsm.create(handles->device, descriptorPool)[0];

        vku::DescriptorSetUpdater dsu;
        dsu.beginDescriptorSet(ds);
        dsu.beginBuffers(0, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        dsu.buffer(vertexBuf.buffer(), 0, totalSize);
        dsu.update(handles->device);

        totalVertCount = (hiddenL.unTriangleCount + hiddenR.unTriangleCount) * 3;
        leftVertCount = hiddenL.unTriangleCount * 3;
    }

    void VRCullMeshRenderer::draw(VkCommandBuffer& cmdBuf) {
        VertPushConstants vpc;
        vpc.viewOffset = leftVertCount;
        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &ds, 0, nullptr);
        vkCmdPushConstants(cmdBuf, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vpc), &vpc);
        vkCmdDraw(cmdBuf, leftVertCount, 1, 0, 0);
    }

    VRCullMeshRenderer::~VRCullMeshRenderer() {
    }
}
