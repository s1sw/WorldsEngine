#include <tracy/Tracy.hpp>
#include "RenderInternal.hpp"
#include "RenderPasses.hpp"

namespace worlds {
    VKRTTPass::VKRTTPass(const RTTPassCreateInfo& ci, VKRenderer* renderer, IVRInterface* vrInterface, uint32_t frameIdx, RenderDebugStats* dbgStats)
        : isVr{ ci.isVr }
        , outputToScreen{ ci.outputToScreen }
        , enableShadows{ ci.enableShadows }
        , cam{ ci.cam }
        , renderer{ renderer }
        , vrInterface{ vrInterface }
        , dbgStats{ dbgStats } {
        createInfo = ci;
        create(renderer, vrInterface, frameIdx, dbgStats);
    }

    void VKRTTPass::create(VKRenderer* renderer, IVRInterface* vrInterface, uint32_t frameIdx, RenderDebugStats* dbgStats) {
        width = createInfo.width;
        height = createInfo.height;

        auto& handles = *renderer->getHandles();
        std::vector<VkDescriptorPoolSize> poolSizes;
        poolSizes.emplace_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 256 * renderer->maxFramesInFlight);
        poolSizes.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256 * renderer->maxFramesInFlight);
        poolSizes.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 256 * renderer->maxFramesInFlight);

        VkDescriptorPoolCreateInfo descriptorPoolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        descriptorPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        descriptorPoolInfo.maxSets = 256;
        descriptorPoolInfo.poolSizeCount = (uint32_t)poolSizes.size();
        descriptorPoolInfo.pPoolSizes = poolSizes.data();
        VKCHECK(vku::createDescriptorPool(handles.device, &descriptorPoolInfo, &descriptorPool));

        VkImageUsageFlags usages =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
            | VK_IMAGE_USAGE_SAMPLED_BIT
            | VK_IMAGE_USAGE_STORAGE_BIT
            | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        TextureResourceCreateInfo polyCreateInfo{
            TextureType::T2DArray,
            VK_FORMAT_B10G11R11_UFLOAT_PACK32,
            (int)width, (int)height,
            usages
        };

        polyCreateInfo.layers = createInfo.isVr ? 2 : 1;
        polyCreateInfo.samples = createInfo.msaaLevel == 0 ? handles.graphicsSettings.msaaLevel : createInfo.msaaLevel;
        hdrTarget = renderer->createTextureResource(polyCreateInfo, "HDR Target");

        TextureResourceCreateInfo depthCreateInfo = polyCreateInfo;
        depthCreateInfo.aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
        depthCreateInfo.format = VK_FORMAT_D32_SFLOAT;
        depthCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        depthTarget = renderer->createTextureResource(depthCreateInfo, "Depth Stencil Image");

        TextureResourceCreateInfo bloomTargetCreateInfo {
            TextureType::T2D,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            (int)createInfo.width, (int)createInfo.height,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT
        };
        bloomTargetCreateInfo.layers = createInfo.isVr ? 2 : 1;

        bloomTarget = renderer->createTextureResource(bloomTargetCreateInfo, "Bloom Target");

        prp = new PolyRenderPass(
            &handles,
            depthTarget,
            hdrTarget,
            bloomTarget,
            createInfo.useForPicking
        );

        TextureResourceCreateInfo finalTargetCreateInfo{
            TextureType::T2D,
            VK_FORMAT_R8G8B8A8_UNORM,
            (int)createInfo.width, (int)createInfo.height,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
        };

        if (!createInfo.outputToScreen) {
            sdrFinalTarget = renderer->createTextureResource(finalTargetCreateInfo, "SDR Target");
        }


        trp = new TonemapRenderPass(
            &handles,
            hdrTarget,
            createInfo.isVr ? renderer->leftEye : createInfo.outputToScreen ? renderer->finalPrePresent : sdrFinalTarget,
            bloomTarget
        );

        VkQueue queue;
        vkGetDeviceQueue(handles.device, handles.graphicsQueueFamilyIdx, 0, &queue);

        vku::executeImmediately(handles.device, handles.commandPool, queue, [&](VkCommandBuffer cmdBuf) {
            hdrTarget->image().setLayout(cmdBuf, VK_IMAGE_LAYOUT_GENERAL);
            if (!createInfo.outputToScreen)
                sdrFinalTarget->image().setLayout(cmdBuf, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            if (createInfo.isVr) {
                renderer->leftEye->image().setLayout(cmdBuf, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
                renderer->rightEye->image().setLayout(cmdBuf, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            }
            });

        entt::registry r;
        RenderContext rCtx{
            .resources = renderer->getResources(),
            .cascadeInfo = {},
            .debugContext = RenderDebugContext {
                .stats = dbgStats
            },
            .passSettings = PassSettings {
                .enableVR = createInfo.isVr,
                .enableShadows = enableShadows,
                .msaaSamples = createInfo.msaaLevel == 0 ? handles.graphicsSettings.msaaLevel : createInfo.msaaLevel
            },
            .registry = r,
            .renderer = renderer,
            .passWidth = width,
            .passHeight = height,
            .frameIndex = frameIdx,
            .maxSimultaneousFrames = renderer->maxFramesInFlight
        };

        trp->setup(rCtx, descriptorPool);
        prp->setup(rCtx, descriptorPool);

        if (isVr) {
            trp->setRightFinalImage(renderer->rightEye);
        }
    }

    void VKRTTPass::destroy() {
        delete prp;
        delete trp;

        delete hdrTarget;
        delete depthTarget;
        delete bloomTarget;

        if (!outputToScreen)
            delete sdrFinalTarget;
    }

    void VKRTTPass::drawNow(entt::registry& world) {
        auto handles = renderer->getHandles();
        VkQueue queue;
        vkGetDeviceQueue(handles->device, handles->graphicsQueueFamilyIdx, 0, &queue);
        vku::executeImmediately(handles->device, handles->commandPool, queue, [&](auto cmdbuf) {
            renderer->uploadSceneAssets(world);
            writeCmds(0, cmdbuf, world);
            });
    }

    void VKRTTPass::requestPick(int x, int y) {
        prp->setPickCoords(x, y);
        prp->requestEntityPick();
    }

    bool VKRTTPass::getPickResult(uint32_t* result) {
        return prp->getPickedEnt(result);
    }

    float* VKRTTPass::getHDRData() {
        auto handles = renderer->getHandles();
        if (isVr) {
            logErr("Getting pass data for VR passes is not supported");
            return nullptr;
        }

        VkImageCreateInfo ici{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        ici.imageType = VK_IMAGE_TYPE_2D;
        ici.extent = VkExtent3D{ width, height, 1 };
        ici.arrayLayers = 1;
        ici.mipLevels = 1;
        ici.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ici.samples = VK_SAMPLE_COUNT_1_BIT;
        ici.tiling = VK_IMAGE_TILING_OPTIMAL;
        ici.usage =
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        vku::GenericImage targetImg{
            handles->device, handles->allocator, ici,
            VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT,
            false, "Transfer Destination" };

        ici.tiling = VK_IMAGE_TILING_OPTIMAL;
        ici.format = VK_FORMAT_B10G11R11_UFLOAT_PACK32;

        vku::GenericImage resolveImg{
            handles->device, handles->allocator, ici,
            VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT,
            false, "Resolve Target" };

        size_t imgSize = width * height * sizeof(float) * 4;
        vku::GenericBuffer outputBuffer{
            handles->device, handles->allocator, VK_BUFFER_USAGE_TRANSFER_DST_BIT, imgSize,
            VMA_MEMORY_USAGE_GPU_TO_CPU, "Output Buffer"
        };

        VkQueue queue;
        vkGetDeviceQueue(handles->device, handles->graphicsQueueFamilyIdx, 0, &queue);

        vku::executeImmediately(handles->device, handles->commandPool, queue, [&](VkCommandBuffer cmdBuf) {
            targetImg.setLayout(cmdBuf,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT);

            resolveImg.setLayout(cmdBuf,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT);

            auto oldHdrLayout = hdrTarget->image().layout();
            hdrTarget->image().setLayout(cmdBuf,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_ACCESS_SHADER_READ_BIT,
                VK_ACCESS_TRANSFER_READ_BIT);

            bool needsResolve = hdrTarget->image().info().samples != VK_SAMPLE_COUNT_1_BIT;
            if (needsResolve) {
                VkImageResolve resolve = { 0 };
                resolve.srcSubresource.layerCount = 1;
                resolve.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

                resolve.dstSubresource.layerCount = 1;
                resolve.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                resolve.extent = VkExtent3D{ width, height, 1 };
                vkCmdResolveImage(
                    cmdBuf,
                    hdrTarget->image().image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    resolveImg.image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1, &resolve);
            }

            resolveImg.setLayout(cmdBuf,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_ACCESS_TRANSFER_READ_BIT);

            VkImageBlit blit = { 0 };
            blit.srcSubresource.layerCount = 1;
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.layerCount = 1;
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcOffsets[1] = blit.dstOffsets[1] =
                VkOffset3D{ static_cast<int32_t>(width), static_cast<int32_t>(height), 1 };

            vkCmdBlitImage(
                cmdBuf,
                needsResolve ? resolveImg.image() : hdrTarget->image().image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                targetImg.image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &blit,
                VK_FILTER_NEAREST);

            hdrTarget->image().setLayout(cmdBuf,
                oldHdrLayout,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                VK_ACCESS_TRANSFER_READ_BIT,
                VK_ACCESS_SHADER_READ_BIT);

            targetImg.setLayout(cmdBuf,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_ACCESS_TRANSFER_READ_BIT);

            VkBufferImageCopy bic = { 0 };
            bic.imageSubresource.layerCount = 1;
            bic.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bic.imageExtent = VkExtent3D{ width, height, 1 };

            vkCmdCopyImageToBuffer(
                cmdBuf,
                targetImg.image(),
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                outputBuffer.buffer(), 1, &bic);
            });

        float* buffer = (float*)malloc(width * height * 4 * sizeof(float));
        char* mapped = (char*)outputBuffer.map(handles->device);
        memcpy(buffer, mapped, width * height * 4 * sizeof(float));
        outputBuffer.unmap(handles->device);

        return buffer;
    }

    void VKRTTPass::writeCmds(uint32_t frameIdx, VkCommandBuffer cmdBuf, entt::registry& world) {
        ZoneScoped;
        RenderResources resources = renderer->getResources();
        RenderContext rCtx{
            .resources = renderer->getResources(),
            .cascadeInfo = {},
            .debugContext = RenderDebugContext {
                .stats = dbgStats
#ifdef TRACY_ENABLE
            , .tracyContexts = &renderer->tracyContexts
#endif
            },
            .passSettings = PassSettings {
                .enableVR = isVr,
                .enableShadows = enableShadows
            },
            .registry = world,
            .cmdBuf = cmdBuf,
            .passWidth = width,
            .passHeight = height,
            .frameIndex = frameIdx
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

        vkCmdUpdateBuffer(cmdBuf, resources.vpMatrixBuffer->buffer(), 0, sizeof(vp), &vp);

        resources.vpMatrixBuffer->barrier(
            cmdBuf,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
            VK_DEPENDENCY_BY_REGION_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_UNIFORM_READ_BIT,
            VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED
        );

        if (enableShadows) {
            renderer->calculateCascadeMatrices(isVr, world, *cam, rCtx);
            renderer->shadowCascadePass->prePass(rCtx);
            renderer->shadowCascadePass->execute(rCtx);
        }

        prp->prePass(rCtx);
        prp->execute(rCtx);

        hdrTarget->image().barrier(cmdBuf,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);

        trp->execute(rCtx);
    }

    VKRTTPass::~VKRTTPass() {
        destroy();
    }
}
