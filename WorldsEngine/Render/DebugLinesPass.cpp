#include "RenderPasses.hpp"
#include "../Physics/Physics.hpp"
#include "Render.hpp"

namespace worlds {
    struct LineVert {
        glm::vec3 pos;
        glm::vec4 col;
    };

    DebugLinesPass::DebugLinesPass(VulkanHandles* handles)
        : handles(handles) {}

    void DebugLinesPass::setup(RenderContext& ctx, vk::RenderPass renderPass) {
        currentLineVBSize = 0;

        vku::DescriptorSetLayoutMaker dslm;
        dslm.buffer(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex, 1);
        lineDsl = dslm.createUnique(handles->device);

        vku::DescriptorSetMaker dsm;
        dsm.layout(*lineDsl);
        lineDs = std::move(dsm.createUnique(handles->device, handles->descriptorPool)[0]);

        vku::PipelineLayoutMaker linePl{};
        linePl.descriptorSetLayout(*lineDsl);
        linePipelineLayout = linePl.createUnique(handles->device);

        vku::DescriptorSetUpdater dsu;
        dsu.beginDescriptorSet(*lineDs);
        dsu.beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer);
        dsu.buffer(ctx.resources.vpMatrixBuffer->buffer(), 0, sizeof(MultiVP));
        dsu.update(handles->device);

        vku::PipelineMaker pm{ ctx.passWidth, ctx.passHeight };
        AssetID vsID = g_assetDB.addOrGetExisting("Shaders/line.vert.spv");
        AssetID fsID = g_assetDB.addOrGetExisting("Shaders/line.frag.spv");

        auto vert = vku::loadShaderAsset(handles->device, vsID);
        auto frag = vku::loadShaderAsset(handles->device, fsID);

        pm.shader(vk::ShaderStageFlagBits::eFragment, frag);
        pm.shader(vk::ShaderStageFlagBits::eVertex, vert);
        pm.vertexBinding(0, (uint32_t)sizeof(LineVert));
        pm.vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(LineVert, pos));
        pm.vertexAttribute(1, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(LineVert, col));
        pm.polygonMode(vk::PolygonMode::eLine);
        pm.lineWidth(4.0f);
        pm.topology(vk::PrimitiveTopology::eLineList);
        pm.depthWriteEnable(true).depthTestEnable(true).depthCompareOp(vk::CompareOp::eGreater);
        pm.subPass(1);

        auto sampleFlags = vku::sampleCountFlags(handles->graphicsSettings.msaaLevel);

        vk::PipelineMultisampleStateCreateInfo pmsci;
        pmsci.rasterizationSamples = sampleFlags;
        pm.multisampleState(pmsci);

        linePipeline = pm.createUnique(
                handles->device, handles->pipelineCache,
                *linePipelineLayout, renderPass);
    }

    void DebugLinesPass::prePass(RenderContext& rCtx) {
        auto& pxRenderBuffer = g_scene->getRenderBuffer();
        uint32_t requiredVBSize = pxRenderBuffer.getNbLines() * 2u;

        if (!lineVB.buffer() || currentLineVBSize < requiredVBSize) {
            currentLineVBSize = requiredVBSize + 128;
            lineVB.destroy();
            lineVB = vku::GenericBuffer{
                handles->device, handles->allocator,
                vk::BufferUsageFlagBits::eVertexBuffer,
                sizeof(LineVert) * currentLineVBSize,
                VMA_MEMORY_USAGE_CPU_TO_GPU, "Line Buffer"
            };
        }

        if (currentLineVBSize > 0) {
            LineVert* lineVBDat = (LineVert*)lineVB.map(handles->device);
            for (uint32_t i = 0; i < pxRenderBuffer.getNbLines(); i++) {
                const auto& physLine = pxRenderBuffer.getLines()[i];
                lineVBDat[(i * 2) + 0] = LineVert{ px2glm(physLine.pos0), glm::vec4(1.0f, 0.0f, 1.0f, 1.0f) };
                lineVBDat[(i * 2) + 1] = LineVert{ px2glm(physLine.pos1), glm::vec4(1.0f, 0.0f, 1.0f, 1.0f) };
            }

            lineVB.unmap(handles->device);
            lineVB.invalidate(handles->device);
            lineVB.flush(handles->device);
            numLineVerts = pxRenderBuffer.getNbLines() * 2;
        }
    }

    void DebugLinesPass::execute(RenderContext& ctx) {
        auto cmdBuf = ctx.cmdBuf;
        if (numLineVerts > 0) {
            cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, *linePipeline);
            cmdBuf.bindVertexBuffers(0, lineVB.buffer(), vk::DeviceSize(0));
            cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *linePipelineLayout, 0, *lineDs, nullptr);
            cmdBuf.draw(numLineVerts, 1, 0, 0);
            ctx.debugContext.stats->numDrawCalls++;
        }
    }
}
