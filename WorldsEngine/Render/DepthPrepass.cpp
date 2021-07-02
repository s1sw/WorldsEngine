#include "RenderPasses.hpp"

namespace worlds {
    DepthPrepass::DepthPrepass(VulkanHandles* handles) : handles(handles) {}

    void DepthPrepass::setup(RenderContext& ctx, vk::RenderPass renderPass, vk::PipelineLayout layout) {
        AssetID vsID = AssetDB::pathToId("Shaders/depth_prepass.vert.spv");
        auto preVertexShader = vku::loadShaderAsset(handles->device, vsID);
        {
            AssetID fsID = AssetDB::pathToId("Shaders/blank.frag.spv");
            auto preFragmentShader = vku::loadShaderAsset(handles->device, fsID);
            vku::PipelineMaker pm{ ctx.passWidth, ctx.passHeight };

            pm.shader(vk::ShaderStageFlagBits::eFragment, preFragmentShader);
            pm.shader(vk::ShaderStageFlagBits::eVertex, preVertexShader);
            pm.vertexBinding(0, (uint32_t)sizeof(Vertex));
            pm.vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, position));
            pm.vertexAttribute(1, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, uv));
            pm.cullMode(vk::CullModeFlagBits::eBack);
            pm.depthWriteEnable(true).depthTestEnable(true).depthCompareOp(vk::CompareOp::eGreater);
            pm.blendBegin(false);
            pm.frontFace(vk::FrontFace::eCounterClockwise);

            vk::PipelineMultisampleStateCreateInfo pmsci;
            pmsci.rasterizationSamples = vku::sampleCountFlags(handles->graphicsSettings.msaaLevel);
            pm.multisampleState(pmsci);
            pm.subPass(0);
            depthPrePipeline = pm.createUnique(handles->device, handles->pipelineCache, layout, renderPass);
        }

        {
            AssetID alphaTestID = AssetDB::pathToId("Shaders/alpha_test_prepass.frag.spv");
            auto alphaTestFragment = vku::loadShaderAsset(handles->device, alphaTestID);
            vku::PipelineMaker pm{ ctx.passWidth, ctx.passHeight };

            pm.shader(vk::ShaderStageFlagBits::eFragment, alphaTestFragment);
            pm.shader(vk::ShaderStageFlagBits::eVertex, preVertexShader);
            pm.vertexBinding(0, (uint32_t)sizeof(Vertex));
            pm.vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, position));
            pm.vertexAttribute(1, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, uv));
            pm.cullMode(vk::CullModeFlagBits::eBack);
            pm.depthWriteEnable(true).depthTestEnable(true).depthCompareOp(vk::CompareOp::eGreater);
            pm.blendBegin(false);
            pm.frontFace(vk::FrontFace::eCounterClockwise);

            vk::PipelineMultisampleStateCreateInfo pmsci;
            pmsci.rasterizationSamples = vku::sampleCountFlags(handles->graphicsSettings.msaaLevel);
            pmsci.alphaToCoverageEnable = true;
            pm.multisampleState(pmsci);
            pm.subPass(0);
            alphaTestPipeline = pm.createUnique(handles->device, handles->pipelineCache, layout, renderPass);
        }
        this->layout = layout;
    }

    void DepthPrepass::prePass(RenderContext& ctx) {
    }

    struct StandardPushConstants {
        uint32_t modelMatrixIdx;
        uint32_t materialIdx;
        uint32_t vpIdx;
        uint32_t objectId;

        glm::vec4 cubemapExt;
        glm::vec4 cubemapPos;

        glm::vec4 texScaleOffset;

        glm::ivec3 screenSpacePickPos;
        uint32_t cubemapIdx;
    };

    void DepthPrepass::execute(RenderContext& ctx, slib::StaticAllocList<SubmeshDrawInfo>& drawInfo) {
        auto& cmdBuf = ctx.cmdBuf;
        cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, *depthPrePipeline);

        bool switchedToAlphaTest = false;

        for (auto& sdi : drawInfo) {
            if (!sdi.opaque) {
                assert(!switchedToAlphaTest);
                switchedToAlphaTest = true;
                cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, *alphaTestPipeline);
            }

            StandardPushConstants pushConst {
                .modelMatrixIdx = sdi.matrixIdx,
                .materialIdx = sdi.materialIdx,
                .vpIdx = 0,
                .objectId = (uint32_t)sdi.ent,
                .texScaleOffset = sdi.texScaleOffset,
                .screenSpacePickPos = glm::ivec3(0, 0, 0)
            };

            cmdBuf.pushConstants<StandardPushConstants>(layout, vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex, 0, pushConst);
            cmdBuf.bindVertexBuffers(0, sdi.vb, vk::DeviceSize(0));
            cmdBuf.bindIndexBuffer(sdi.ib, 0, vk::IndexType::eUint32);
            cmdBuf.drawIndexed(sdi.indexCount, 1, sdi.indexOffset, 0, 0);
            ctx.debugContext.stats->numDrawCalls++;
        }
    }
}
