#include "Render.hpp"
#include "RenderPasses.hpp"

namespace worlds {
    RTTPass::RTTPass(const RTTPassCreateInfo& ci, VKRenderer* renderer, IVRInterface* vrInterface, uint32_t frameIdx, RenderDebugStats* dbgStats, ShadowCascadePass* scp, RenderTexture* finalPrePresent, RenderTexture* finalPrePresentR)
        : width {ci.width}
        , height {ci.height}
        , isVr {ci.isVr}
        , outputToScreen {ci.outputToScreen}
        , enableShadows {ci.enableShadows}
        , cam {ci.cam}
        , renderer {renderer}
        , dbgStats {dbgStats}
        , shadowCascadePass {scp} {
        auto& handles = *renderer->getHandles();
        RenderResources resources = renderer->getResources();

        vk::ImageCreateInfo ici;
        ici.imageType = vk::ImageType::e2D;
        ici.extent = vk::Extent3D{ ci.width, ci.height, 1 };
        ici.arrayLayers = ci.isVr ? 2 : 1;
        ici.mipLevels = 1;
        ici.format = vk::Format::eB10G11R11UfloatPack32;
        ici.initialLayout = vk::ImageLayout::eUndefined;
        ici.samples = vku::sampleCountFlags(handles.graphicsSettings.msaaLevel);
        ici.usage =
              vk::ImageUsageFlagBits::eColorAttachment
            | vk::ImageUsageFlagBits::eSampled
            | vk::ImageUsageFlagBits::eStorage
            | vk::ImageUsageFlagBits::eTransferSrc;

        RTResourceCreateInfo polyCreateInfo{ ici, vk::ImageViewType::e2DArray, vk::ImageAspectFlagBits::eColor };
        hdrTarget = renderer->createRTResource(polyCreateInfo, "HDR Target");

        ici.format = vk::Format::eD32Sfloat;
        ici.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;
        RTResourceCreateInfo depthCreateInfo{ ici, vk::ImageViewType::e2DArray, vk::ImageAspectFlagBits::eDepth };
        depthTarget = renderer->createRTResource(depthCreateInfo, "Depth Stencil Image");

        prp = new PolyRenderPass(
            &handles,
            depthTarget,
            hdrTarget,
            resources.shadowCascades,
            ci.useForPicking
        );

        ici.samples = vk::SampleCountFlagBits::e1;
        ici.format = vk::Format::eR8Unorm;
        ici.usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled;

        ici.arrayLayers = 1;
        ici.format = vk::Format::eR8G8B8A8Unorm;
        ici.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled;

        if (!ci.outputToScreen) {
            RTResourceCreateInfo sdrTarget{ ici, vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eColor };
            sdrFinalTarget = renderer->createRTResource(sdrTarget, "SDR Target");
        }

        trp = new TonemapRenderPass(
            &handles,
            hdrTarget,
            ci.outputToScreen ? finalPrePresent : sdrFinalTarget
        );

        auto queue = handles.device.getQueue(handles.graphicsQueueFamilyIdx, 0);

        vku::executeImmediately(handles.device, handles.commandPool, queue, [&](vk::CommandBuffer cmdBuf) {
            hdrTarget->image.setLayout(cmdBuf, vk::ImageLayout::eGeneral);
            if (!ci.outputToScreen)
                sdrFinalTarget->image.setLayout(cmdBuf, vk::ImageLayout::eShaderReadOnlyOptimal);
            if (ci.isVr) {
                finalPrePresentR->image.setLayout(cmdBuf, vk::ImageLayout::eTransferSrcOptimal);
            }
        });

        entt::registry r;
        RenderContext rCtx {
            .resources = renderer->getResources(),
            .cascadeInfo = {},
            .debugContext = RenderDebugContext {
                .stats = dbgStats
            },
            .passSettings = PassSettings {
                .enableVR = ci.isVr,
                .enableShadows = enableShadows
            },
            .registry = r,
            .passWidth = ci.width,
            .passHeight = ci.height,
            .imageIndex = frameIdx
        };

        trp->setup(rCtx);
        prp->setup(rCtx);

        if (ci.isVr) {
            trp->setRightFinalImage(finalPrePresentR);
        }
    }

    void RTTPass::drawNow(entt::registry& world) {
        auto handles = renderer->getHandles();
        auto queue = handles->device.getQueue(handles->graphicsQueueFamilyIdx, 0);
        vku::executeImmediately(handles->device, handles->commandPool, queue, [&](auto cmdbuf) {
            renderer->uploadSceneAssets(world);
            writeCmds(0, cmdbuf, world);
        });
    }

    void RTTPass::requestPick(int x, int y) {
        prp->setPickCoords(x, y);
        prp->requestEntityPick();
    }

    bool RTTPass::getPickResult(uint32_t* result) {
        return prp->getPickedEnt(result);
    }

    float* RTTPass::getHDRData() {
        auto handles = renderer->getHandles();
        if (isVr) {
            logErr("Getting pass data for VR passes is not supported");
            return nullptr;
        }

        vk::ImageCreateInfo ici;
        ici.imageType = vk::ImageType::e2D;
        ici.extent = vk::Extent3D{ width, height, 1 };
        ici.arrayLayers = 1;
        ici.mipLevels = 1;
        ici.format = vk::Format::eR32G32B32A32Sfloat;
        ici.initialLayout = vk::ImageLayout::eUndefined;
        ici.samples = vk::SampleCountFlagBits::e1;
        ici.tiling = vk::ImageTiling::eOptimal;
        ici.usage =
            vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc;

        vku::GenericImage targetImg{
            handles->device, handles->allocator, ici,
            vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eColor,
            false, "Transfer Destination" };

        ici.tiling = vk::ImageTiling::eOptimal;
        ici.format = vk::Format::eB10G11R11UfloatPack32;

        vku::GenericImage resolveImg{
            handles->device, handles->allocator, ici,
            vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eColor,
            false, "Resolve Target" };

        size_t imgSize = width * height * sizeof(float) * 4;
        vku::GenericBuffer outputBuffer {
            handles->device, handles->allocator, vk::BufferUsageFlagBits::eTransferDst, imgSize,
            VMA_MEMORY_USAGE_GPU_TO_CPU, "Output Buffer"
        };

        auto queue = handles->device.getQueue(handles->graphicsQueueFamilyIdx, 0);

        vku::executeImmediately(handles->device, handles->commandPool, queue, [&](vk::CommandBuffer cmdBuf) {
            targetImg.setLayout(cmdBuf,
                    vk::ImageLayout::eTransferDstOptimal,
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::AccessFlagBits::eTransferWrite,
                    vk::AccessFlagBits::eTransferWrite);

            resolveImg.setLayout(cmdBuf,
                    vk::ImageLayout::eTransferDstOptimal,
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::AccessFlagBits::eTransferWrite,
                    vk::AccessFlagBits::eTransferWrite);

            auto oldHdrLayout = hdrTarget->image.layout();
            hdrTarget->image.setLayout(cmdBuf,
                    vk::ImageLayout::eTransferSrcOptimal,
                    vk::PipelineStageFlagBits::eAllGraphics,
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::AccessFlagBits::eShaderRead,
                    vk::AccessFlagBits::eTransferRead);

            vk::ImageResolve resolve;
            resolve.srcSubresource.layerCount = 1;
            resolve.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
            resolve.dstSubresource.layerCount = 1;
            resolve.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
            resolve.extent = vk::Extent3D { width, height, 1 };
            cmdBuf.resolveImage(
                    hdrTarget->image.image(), vk::ImageLayout::eTransferSrcOptimal,
                    resolveImg.image(), vk::ImageLayout::eTransferDstOptimal,
                    resolve);

            resolveImg.setLayout(cmdBuf,
                    vk::ImageLayout::eTransferSrcOptimal,
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::AccessFlagBits::eTransferWrite,
                    vk::AccessFlagBits::eTransferRead);

            vk::ImageBlit blit;
            blit.srcSubresource.layerCount = 1;
            blit.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
            blit.dstSubresource.layerCount = 1;
            blit.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
            blit.srcOffsets[1] = blit.dstOffsets[1] =
                vk::Offset3D {static_cast<int32_t>(width), static_cast<int32_t>(height), 1};

            cmdBuf.blitImage(
                    resolveImg.image(), vk::ImageLayout::eTransferSrcOptimal,
                    targetImg.image(), vk::ImageLayout::eTransferDstOptimal,
                    1,
                    &blit,
                    vk::Filter::eNearest);

            hdrTarget->image.setLayout(cmdBuf,
                    oldHdrLayout,
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::PipelineStageFlagBits::eAllGraphics,
                    vk::AccessFlagBits::eTransferRead,
                    vk::AccessFlagBits::eShaderRead);

            targetImg.setLayout(cmdBuf,
                    vk::ImageLayout::eTransferSrcOptimal,
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::AccessFlagBits::eTransferWrite,
                    vk::AccessFlagBits::eTransferRead);

            vk::BufferImageCopy bic;
            bic.imageSubresource.layerCount = 1;
            bic.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
            bic.imageExtent = vk::Extent3D { width, height, 1 };

            cmdBuf.copyImageToBuffer(
                targetImg.image(),
                vk::ImageLayout::eTransferSrcOptimal,
                outputBuffer.buffer(), bic);
        });

        float* buffer = (float*)malloc(width * height * 4 * sizeof(float));
        char* mapped = (char*)outputBuffer.map(handles->device);
        memcpy(buffer, mapped, width * height * 4 * sizeof(float));
        outputBuffer.unmap(handles->device);

        return buffer;
    }

    void RTTPass::writeCmds(uint32_t frameIdx, vk::CommandBuffer cmdBuf, entt::registry& world) {
        RenderResources resources = renderer->getResources();
        RenderContext rCtx {
            .resources = renderer->getResources(),
            .cascadeInfo = {},
            .debugContext = RenderDebugContext {
                .stats = dbgStats
            },
            .passSettings = PassSettings {
                .enableVR = isVr,
                .enableShadows = enableShadows
            },
            .registry = world,
            .cmdBuf = cmdBuf,
            .passWidth = width,
            .passHeight = height,
            .imageIndex = frameIdx
        };

        if (isVr) {
            glm::mat4 headViewMatrix = vrInterface->getHeadTransform(renderer->vrPredictAmount);

            glm::mat4 viewMats[2] = {
                vrInterface->getEyeViewMatrix(Eye::LeftEye),
                vrInterface->getEyeViewMatrix(Eye::RightEye)
            };

            glm::mat4 projMats[2] = {
                vrInterface->getEyeProjectionMatrix(Eye::LeftEye, cam->near),
                vrInterface->getEyeProjectionMatrix(Eye::RightEye, cam->near)
            };

            for (int i = 0; i < 2; i++) {
                rCtx.viewMatrices[i] = glm::inverse(headViewMatrix * viewMats[i]) * cam->getViewMatrix();
                rCtx.projMatrices[i] = projMats[i];
            }
        } else {
            rCtx.projMatrices[0] = cam->getProjectionMatrix((float)width / (float)height);
            rCtx.viewMatrices[0] = cam->getViewMatrix();
        }

        MultiVP vp;
        for (int i = 0; i < 2; i++) {
            vp.projections[i] = rCtx.projMatrices[i];
            vp.views[i] = rCtx.viewMatrices[i];
            vp.viewPos[i] = glm::inverse(vp.views[i])[3];
        }

        cmdBuf.updateBuffer(resources.vpMatrixBuffer->buffer(), 0, sizeof(vp), &vp);

        if (enableShadows) {
            renderer->calculateCascadeMatrices(world, *cam, rCtx);
            shadowCascadePass->prePass(rCtx);
            shadowCascadePass->execute(rCtx);
        }

        prp->prePass(rCtx);
        prp->execute(rCtx);

        hdrTarget->image.barrier(cmdBuf,
            vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eComputeShader,
            vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eShaderRead);

        trp->execute(rCtx);
    }

    RTTPass::~RTTPass() {
        renderer->device->waitIdle();

        delete prp;
        delete trp;

        delete hdrTarget;
        delete depthTarget;

        if (!outputToScreen)
            delete sdrFinalTarget;
    }
}
