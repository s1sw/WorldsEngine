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

    void DebugLinesPass::setup(RenderContext& ctx, VkRenderPass renderPass, VkDescriptorPool descriptorPool) {
        currentLineVBSize = 0;

        vku::DescriptorSetLayoutMaker dslm;
        dslm.buffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1);
        lineDsl = dslm.create(handles->device);

        vku::DescriptorSetMaker dsm;
        dsm.layout(lineDsl);
        lineDs = dsm.create(handles->device, descriptorPool)[0];

        vku::PipelineLayoutMaker linePl{};
        linePl.descriptorSetLayout(lineDsl);
        linePipelineLayout = linePl.create(handles->device);

        vku::DescriptorSetUpdater dsu;
        dsu.beginDescriptorSet(lineDs);
        dsu.beginBuffers(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        dsu.buffer(ctx.resources.vpMatrixBuffer->buffer(), 0, sizeof(MultiVP));
        dsu.update(handles->device);

        vku::PipelineMaker pm{ ctx.passWidth, ctx.passHeight };
        AssetID vsID = AssetDB::pathToId("Shaders/line.vert.spv");
        AssetID fsID = AssetDB::pathToId("Shaders/line.frag.spv");

        auto vert = vku::loadShaderAsset(handles->device, vsID);
        auto frag = vku::loadShaderAsset(handles->device, fsID);

        pm.shader(VK_SHADER_STAGE_FRAGMENT_BIT, frag);
        pm.shader(VK_SHADER_STAGE_VERTEX_BIT, vert);
        pm.vertexBinding(0, (uint32_t)sizeof(LineVert));
        pm.vertexAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(LineVert, pos));
        pm.vertexAttribute(1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(LineVert, col));
        pm.polygonMode(VK_POLYGON_MODE_LINE);
        pm.lineWidth(4.0f);
        pm.topology(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
        pm.depthWriteEnable(true).depthTestEnable(true).depthCompareOp(VK_COMPARE_OP_GREATER);
        pm.subPass(1);

        auto sampleFlags = vku::sampleCountFlags(ctx.passSettings.msaaSamples);
        pm.rasterizationSamples(sampleFlags);

        linePipeline = pm.create(
            handles->device, handles->pipelineCache,
            linePipelineLayout, renderPass);
    }

    void DebugLinesPass::prePass(RenderContext& rCtx) {
        auto& pxRenderBuffer = g_scene->getRenderBuffer();
        uint32_t requiredVBSize = pxRenderBuffer.getNbLines() * 2u * sizeof(LineVert);

        if (!lineVB.buffer() || currentLineVBSize < requiredVBSize) {
            currentLineVBSize = requiredVBSize + 128;
            lineVB.destroy();
            lineVB = vku::GenericBuffer{
                handles->device, handles->allocator,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
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
            vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, linePipeline);
            VkBuffer buffer = lineVB.buffer();
            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(cmdBuf, 0, 1, &buffer, &offset);
            vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, linePipelineLayout, 0, 1, &lineDs, 0, nullptr);
            vkCmdDraw(cmdBuf, numLineVerts, 1, 0, 0);
            ctx.debugContext.stats->numDrawCalls++;
        }
    }

    DebugLinesPass::~DebugLinesPass() {
        vkDestroyPipeline(handles->device, linePipeline, nullptr);
        vkDestroyPipelineLayout(handles->device, linePipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(handles->device, lineDsl, nullptr);
        lineVB.destroy();
    }
}
