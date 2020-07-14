#include "Engine.hpp"
#ifdef TRACY_ENABLE
#include "tracy/Tracy.hpp"
#include "tracy/TracyC.h"
#include "tracy/TracyVulkan.hpp"
#endif
#include "imgui_impl_vulkan.h"
#include "Transform.hpp"

void VKRenderer::renderShadowmap(RenderCtx& ctx) {
#ifdef TRACY_ENABLE
    ZoneScoped;
    TracyVkZone(tracyContexts[ctx.imageIndex], *ctx.cmdBuf, "Shadowmap");
#endif
    vk::ClearDepthStencilValue clearDepthValue{ 1.0f, 0 };
    std::array<vk::ClearValue, 1> clearColours{ clearDepthValue };

    vk::RenderPassBeginInfo rpbi;

    rpbi.renderPass = *shadowmapPass;
    rpbi.framebuffer = *shadowmapFb;
    rpbi.renderArea = vk::Rect2D{ {0, 0}, {shadowmapRes, shadowmapRes} };
    rpbi.clearValueCount = (uint32_t)clearColours.size();
    rpbi.pClearValues = clearColours.data();

    vk::UniqueCommandBuffer& cmdBuf = ctx.cmdBuf;
    Camera& cam = ctx.cam;
    entt::registry& reg = ctx.reg;


    cmdBuf->beginRenderPass(rpbi, vk::SubpassContents::eInline);
    cmdBuf->bindPipeline(vk::PipelineBindPoint::eGraphics, *shadowmapPipeline);

    glm::mat4 shadowmapMatrix;
    glm::vec3 viewPos = cam.position;

    reg.view<WorldLight, Transform>().each([&shadowmapMatrix, &viewPos](auto ent, WorldLight& l, Transform& transform) {
        glm::vec3 lightForward = glm::normalize(transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f));
        if (l.type == LightType::Directional) {
            const float SHADOW_DISTANCE = 50.0f;
            glm::vec3 shadowMapPos = glm::round(viewPos - (transform.rotation * glm::vec3(0.0f, 0.f, 50.0f)));
            glm::mat4 proj = glm::orthoZO(
                -SHADOW_DISTANCE, SHADOW_DISTANCE,
                -SHADOW_DISTANCE, SHADOW_DISTANCE,
                1.0f, 1000.f);

            glm::mat4 view = glm::lookAt(
                shadowMapPos,
                shadowMapPos - lightForward,
                glm::vec3(0.0f, 1.0f, 0.0));

            shadowmapMatrix = proj * view;
        }
        });

    reg.view<Transform, WorldObject>().each([this, &cmdBuf, &cam, &shadowmapMatrix](auto ent, Transform& transform, WorldObject& obj) {
        auto meshPos = loadedMeshes.find(obj.mesh);

        if (meshPos == loadedMeshes.end()) {
            // Haven't loaded the mesh yet
            return;
        }

        glm::mat4 model = transform.getMatrix();
        glm::mat4 mvp = shadowmapMatrix * model;
        cmdBuf->pushConstants<glm::mat4>(*shadowmapPipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, mvp);
        cmdBuf->bindVertexBuffers(0, meshPos->second.vb.buffer(), vk::DeviceSize(0));
        cmdBuf->bindIndexBuffer(meshPos->second.ib.buffer(), 0, meshPos->second.indexType);
        cmdBuf->drawIndexed(meshPos->second.indexCount, 1, 0, 0, 0);
        });

    reg.view<Transform, ProceduralObject>().each([this, &cmdBuf, &cam, &shadowmapMatrix](auto ent, Transform& transform, ProceduralObject& obj) {
        if (!obj.visible) return;
        glm::mat4 model = transform.getMatrix();
        glm::mat4 mvp = shadowmapMatrix * model;
        cmdBuf->pushConstants<glm::mat4>(*shadowmapPipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, mvp);
        cmdBuf->bindVertexBuffers(0, obj.vb.buffer(), vk::DeviceSize(0));
        cmdBuf->bindIndexBuffer(obj.ib.buffer(), 0, obj.indexType);
        cmdBuf->drawIndexed(obj.indexCount, 1, 0, 0, 0);
        });

    cmdBuf->endRenderPass();
}

void VKRenderer::renderPolys(RenderCtx& ctx) {
#ifdef TRACY_ENABLE
    ZoneScoped;
    TracyVkZone(tracyContexts[ctx.imageIndex], *ctx.cmdBuf, "Polys");
#endif
    // Fast path clear values for AMD
    std::array<float, 4> clearColorValue{ 0.0f, 0.0f, 0.0f, 1 };
    vk::ClearDepthStencilValue clearDepthValue{ 1.0f, 0 };
    std::array<vk::ClearValue, 2> clearColours{ vk::ClearValue{clearColorValue}, clearDepthValue };
    vk::RenderPassBeginInfo rpbi;

    rpbi.renderPass = *renderPass;
    rpbi.framebuffer = *renderFb;
    rpbi.renderArea = vk::Rect2D{ {0, 0}, {width, height} };
    rpbi.clearValueCount = (uint32_t)clearColours.size();
    rpbi.pClearValues = clearColours.data();

    vk::UniqueCommandBuffer& cmdBuf = ctx.cmdBuf;
    entt::registry& reg = ctx.reg;
    Camera& cam = ctx.cam;

    cmdBuf->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, descriptorSets[0], nullptr);
    cmdBuf->beginRenderPass(rpbi, vk::SubpassContents::eInline);
    cmdBuf->bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);

    int matrixIdx = 0;

    reg.view<Transform, WorldObject>().each([this, &cmdBuf, &cam, &matrixIdx](auto ent, Transform& transform, WorldObject& obj) {
        auto meshPos = loadedMeshes.find(obj.mesh);

        if (meshPos == loadedMeshes.end()) {
            // Haven't loaded the mesh yet
            return;
        }

        StandardPushConstants pushConst{ glm::vec4(cam.position, 0.0f), obj.texScaleOffset, glm::ivec4(matrixIdx, obj.materialIndex, 0, 0) };
        cmdBuf->pushConstants<StandardPushConstants>(*pipelineLayout, vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex, 0, pushConst);
        cmdBuf->bindVertexBuffers(0, meshPos->second.vb.buffer(), vk::DeviceSize(0));
        cmdBuf->bindIndexBuffer(meshPos->second.ib.buffer(), 0, meshPos->second.indexType);
        cmdBuf->drawIndexed(meshPos->second.indexCount, 1, 0, 0, 0);
        matrixIdx++;
        });

    reg.view<Transform, ProceduralObject>().each([this, &cmdBuf, &cam, &matrixIdx](auto ent, Transform& transform, ProceduralObject& obj) {
        if (!obj.visible) return;
        StandardPushConstants pushConst{ glm::vec4(cam.position, 0.0f), glm::vec4(1.0f, 1.0f, 0.0f, 0.0f), glm::ivec4(matrixIdx, 0, 0, 0) };
        cmdBuf->pushConstants<StandardPushConstants>(*pipelineLayout, vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex, 0, pushConst);
        cmdBuf->bindVertexBuffers(0, obj.vb.buffer(), vk::DeviceSize(0));
        cmdBuf->bindIndexBuffer(obj.ib.buffer(), 0, obj.indexType);
        cmdBuf->drawIndexed(obj.indexCount, 1, 0, 0, 0);
        matrixIdx++;
        });

    cmdBuf->endRenderPass();
}

void VKRenderer::doTonemap(RenderCtx& ctx) {
#ifdef TRACY_ENABLE
    ZoneScoped;
    TracyVkZone(tracyContexts[ctx.imageIndex], *ctx.cmdBuf, "Tonemap/Postprocessing");
#endif
    auto& cmdBuf = ctx.cmdBuf;
    finalPrePresent.setLayout(*cmdBuf, vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::AccessFlagBits::eTransferRead, vk::AccessFlagBits::eShaderWrite);

    //::imageBarrier(*cmdBuf, rtResources.at(polyImage).image.image(), vk::ImageLayout::eGeneral, vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eShaderRead, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eComputeShader);

    cmdBuf->bindDescriptorSets(vk::PipelineBindPoint::eCompute, *tonemapPipelineLayout, 0, tonemapDescriptorSet, nullptr);
    cmdBuf->bindPipeline(vk::PipelineBindPoint::eCompute, *tonemapPipeline);

    cmdBuf->dispatch((width + 15) / 16, (height + 15) / 16, 1);

    vku::transitionLayout(*cmdBuf, finalPrePresent.image(),
        vk::ImageLayout::eGeneral, vk::ImageLayout::eColorAttachmentOptimal,
        vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eColorAttachmentRead);

    std::array<float, 4> clearColorValue{ 0.0f, 0.0f, 0.0f, 1 };
    std::array<vk::ClearValue, 1> clearColours{ vk::ClearValue{clearColorValue} };
    vk::RenderPassBeginInfo rpbi;
    rpbi.renderPass = *imguiRenderPass;
    rpbi.framebuffer = *finalPrePresentFB;
    rpbi.renderArea = vk::Rect2D{ {0, 0}, {width, height} };
    rpbi.clearValueCount = clearColours.size();
    rpbi.pClearValues = clearColours.data();
    cmdBuf->beginRenderPass(rpbi, vk::SubpassContents::eInline);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *cmdBuf);
    cmdBuf->endRenderPass();

    // account for implicit renderpass transition
    finalPrePresent.setCurrentLayout(vk::ImageLayout::eTransferSrcOptimal);
}