#include "RenderPasses.hpp"
#include "../Core/Engine.hpp"
#include "../Core/Transform.hpp"
#include <openvr.h>
#include "tracy/Tracy.hpp"
#include "Render.hpp"
#include "../Physics/Physics.hpp"
#include "Frustum.hpp"
#include "../Core/Console.hpp"
#include "ShaderCache.hpp"
#include <slib/StaticAllocList.hpp>

namespace worlds {
    ConVar showWireframe("r_wireframeMode", "0", "0 - No wireframe; 1 - Wireframe only; 2 - Wireframe + solid");
    ConVar dbgDrawMode("r_dbgDrawMode", "0", "0 = Normal, 1 = Normals, 2 = Metallic, 3 = Roughness, 4 = AO");

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

    struct SkyboxPushConstants {
        // (x: vp index, y: cubemap index)
        glm::ivec4 ubIndices;
    };

    struct PickingBuffer {
        uint32_t objectID;
    };

    struct PickBufCSPushConstants {
        uint32_t clearObjId;
        uint32_t doPicking;
    };

    struct LineVert {
        glm::vec3 pos;
        glm::vec4 col;
    };

    void PolyRenderPass::updateDescriptorSets(PassSetupCtx& ctx) {
        ZoneScoped;
        auto& texSlots = ctx.slotArrays.textures;
        auto& cubemapSlots = ctx.slotArrays.cubemaps;
        {
            vku::DescriptorSetUpdater updater(10, 128, 0);
            updater.beginDescriptorSet(*descriptorSet);

            updater.beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer);
            updater.buffer(this->vpUB.buffer(), 0, sizeof(MultiVP));

            updater.beginBuffers(1, 0, vk::DescriptorType::eUniformBuffer);
            updater.buffer(lightsUB.buffer(), 0, sizeof(LightUB));

            updater.beginBuffers(2, 0, vk::DescriptorType::eStorageBuffer);
            updater.buffer(ctx.materialUB->buffer(), 0, sizeof(MaterialsUB));

            updater.beginBuffers(3, 0, vk::DescriptorType::eStorageBuffer);
            updater.buffer(modelMatrixUB.buffer(), 0, sizeof(ModelMatrices));

            for (uint32_t i = 0; i < texSlots.size(); i++) {
                if (texSlots.isSlotPresent(i)) {
                    updater.beginImages(4, i, vk::DescriptorType::eCombinedImageSampler);
                    updater.image(*albedoSampler, texSlots[i].imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
                }
            }

            updater.beginImages(5, 0, vk::DescriptorType::eCombinedImageSampler);
            updater.image(*shadowSampler, shadowImage->image.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

            for (uint32_t i = 0; i < cubemapSlots.size(); i++) {
                if (cubemapSlots.isSlotPresent(i)) {
                    updater.beginImages(6, i, vk::DescriptorType::eCombinedImageSampler);
                    updater.image(*albedoSampler, cubemapSlots[i].imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
                }
            }

            updater.beginImages(7, 0, vk::DescriptorType::eCombinedImageSampler);
            updater.image(*albedoSampler, ctx.brdfLut->imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

            updater.beginBuffers(8, 0, vk::DescriptorType::eStorageBuffer);
            updater.buffer(pickingBuffer.buffer(), 0, sizeof(PickingBuffer));

            if (!updater.ok())
                fatalErr("updater was not ok");

            updater.update(ctx.vkCtx.device);
        }

        {
            vku::DescriptorSetUpdater updater;
            updater.beginDescriptorSet(*skyboxDs);
            updater.beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer);
            updater.buffer(vpUB.buffer(), 0, sizeof(MultiVP));

            updater.beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler);
            updater.image(*albedoSampler, cubemapSlots[lastSky].imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

            updater.update(ctx.vkCtx.device);
        }

        dsUpdateNeeded = false;
    }

    PolyRenderPass::PolyRenderPass(
        RenderTexture* depthStencilImage,
        RenderTexture* polyImage,
        RenderTexture* shadowImage,
        bool enablePicking)
        : depthStencilImage(depthStencilImage)
        , polyImage(polyImage)
        , shadowImage(shadowImage)
        , enablePicking(enablePicking)
        , pickX(0)
        , pickY(0)
        , pickThisFrame(false)
        , awaitingResults(false)
        , setEventNextFrame(false) {

    }

    static ConVar depthPrepass("r_depthPrepass", "0");
    static ConVar enableParallaxMapping("r_doParallaxMapping", "0");
    static ConVar maxParallaxLayers("r_maxParallaxLayers", "32");
    static ConVar minParallaxLayers("r_minParallaxLayers", "4");

    void PolyRenderPass::setup(PassSetupCtx& psCtx) {
        ZoneScoped;
        auto& ctx = psCtx.vkCtx;

        vku::SamplerMaker sm{};
        sm.magFilter(vk::Filter::eLinear).minFilter(vk::Filter::eLinear).mipmapMode(vk::SamplerMipmapMode::eLinear).anisotropyEnable(true).maxAnisotropy(16.0f).maxLod(VK_LOD_CLAMP_NONE).minLod(0.0f);
        albedoSampler = sm.createUnique(ctx.device);

        vku::SamplerMaker ssm{};
        ssm.magFilter(vk::Filter::eLinear).minFilter(vk::Filter::eLinear).mipmapMode(vk::SamplerMipmapMode::eLinear).compareEnable(true).compareOp(vk::CompareOp::eLessOrEqual);
        shadowSampler = ssm.createUnique(ctx.device);

        vku::DescriptorSetLayoutMaker dslm;
        // VP
        dslm.buffer(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 1);
        // Lights
        dslm.buffer(1, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex, 1);
        // Materials
        dslm.buffer(2, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eFragment, 1);
        // Model matrices
        dslm.buffer(3, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eVertex, 1);
        // Textures
        dslm.image(4, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, NUM_TEX_SLOTS);
        dslm.bindFlag(4, vk::DescriptorBindingFlagBits::ePartiallyBound);
        // Shadowmap
        dslm.image(5, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1);
        // Cubemaps
        dslm.image(6, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, NUM_CUBEMAP_SLOTS);
        dslm.bindFlag(6, vk::DescriptorBindingFlagBits::ePartiallyBound);
        // BRDF LUT
        dslm.image(7, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1);
        // Picking
        dslm.buffer(8, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eFragment, 1);

        dsl = dslm.createUnique(ctx.device);

        vku::PipelineLayoutMaker plm;
        plm.pushConstantRange(vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex, 0, sizeof(StandardPushConstants));
        plm.descriptorSetLayout(*dsl);
        pipelineLayout = plm.createUnique(ctx.device);

        vpUB = vku::UniformBuffer(
                ctx.device, ctx.allocator, sizeof(MultiVP),
                VMA_MEMORY_USAGE_CPU_TO_GPU, "VP");

        lightsUB = vku::UniformBuffer(
                ctx.device, ctx.allocator, sizeof(LightUB),
                VMA_MEMORY_USAGE_CPU_TO_GPU, "Lights");

        modelMatrixUB = vku::GenericBuffer(
                ctx.device, ctx.allocator,
                vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
                sizeof(ModelMatrices), VMA_MEMORY_USAGE_CPU_TO_GPU, "Model matrices");

        pickingBuffer = vku::GenericBuffer(
                ctx.device, ctx.allocator,
                vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
                sizeof(PickingBuffer), VMA_MEMORY_USAGE_CPU_ONLY, "Picking buffer");

        modelMatricesMapped = (ModelMatrices*)modelMatrixUB.map(ctx.device);
        lightMapped = (LightUB*)lightsUB.map(ctx.device);
        vpMapped = (MultiVP*)vpUB.map(ctx.device);

        pickEvent = ctx.device.createEventUnique(vk::EventCreateInfo{});

        vku::DescriptorSetMaker dsm;
        dsm.layout(*dsl);
        descriptorSet = std::move(dsm.createUnique(ctx.device, ctx.descriptorPool)[0]);

        vku::RenderpassMaker rPassMaker;

        rPassMaker.attachmentBegin(vk::Format::eB10G11R11UfloatPack32);
        rPassMaker.attachmentLoadOp(vk::AttachmentLoadOp::eDontCare);
        rPassMaker.attachmentStoreOp(vk::AttachmentStoreOp::eStore);
        rPassMaker.attachmentSamples(polyImage->image.info().samples);
        rPassMaker.attachmentFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

        rPassMaker.attachmentBegin(vk::Format::eD32Sfloat);
        rPassMaker.attachmentLoadOp(vk::AttachmentLoadOp::eClear);
        rPassMaker.attachmentStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
        rPassMaker.attachmentSamples(polyImage->image.info().samples);
        rPassMaker.attachmentFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

        rPassMaker.subpassBegin(vk::PipelineBindPoint::eGraphics);
        rPassMaker.subpassDepthStencilAttachment(vk::ImageLayout::eDepthStencilAttachmentOptimal, 1);

        rPassMaker.dependencyBegin(VK_SUBPASS_EXTERNAL, 0);
        rPassMaker.dependencySrcStageMask(vk::PipelineStageFlagBits::eEarlyFragmentTests);
        rPassMaker.dependencyDstStageMask(vk::PipelineStageFlagBits::eEarlyFragmentTests);
        rPassMaker.dependencyDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite);

        rPassMaker.subpassBegin(vk::PipelineBindPoint::eGraphics);
        rPassMaker.subpassColorAttachment(vk::ImageLayout::eColorAttachmentOptimal, 0);
        rPassMaker.subpassDepthStencilAttachment(vk::ImageLayout::eDepthStencilAttachmentOptimal, 1);

        rPassMaker.dependencyBegin(0, 1);
        rPassMaker.dependencySrcStageMask(vk::PipelineStageFlagBits::eEarlyFragmentTests);
        rPassMaker.dependencyDstStageMask(vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eColorAttachmentOutput);
        rPassMaker.dependencyDstAccessMask(vk::AccessFlagBits::eColorAttachmentRead |
                                           vk::AccessFlagBits::eColorAttachmentWrite |
                                           vk::AccessFlagBits::eDepthStencilAttachmentRead |
                                           vk::AccessFlagBits::eDepthStencilAttachmentWrite);


        // AMD driver bug workaround: shaders that use ViewIndex without a multiview renderpass
        // will crash the driver, so we always set up a renderpass with multiview even if it's only
        // one view.
        vk::RenderPassMultiviewCreateInfo renderPassMultiviewCI{};
        uint32_t viewMasks[2] = { 0b00000001, 0b00000001 };
        uint32_t correlationMask = 0b00000001;

        if (psCtx.enableVR) {
            viewMasks[0] = 0b00000011;
            viewMasks[1] = 0b00000011;
            correlationMask = 0b00000011;
        }

        renderPassMultiviewCI.subpassCount = 2;
        renderPassMultiviewCI.pViewMasks = viewMasks;
        renderPassMultiviewCI.correlationMaskCount = 1;
        renderPassMultiviewCI.pCorrelationMasks = &correlationMask;
        rPassMaker.setPNext(&renderPassMultiviewCI);

        renderPass = rPassMaker.createUnique(ctx.device);

        vk::ImageView attachments[2] = { polyImage->image.imageView(), depthStencilImage->image.imageView() };

        auto extent = polyImage->image.info().extent;
        vk::FramebufferCreateInfo fci;
        fci.attachmentCount = 2;
        fci.pAttachments = attachments;
        fci.width = extent.width;
        fci.height = extent.height;
        fci.renderPass = *this->renderPass;
        fci.layers = 1;
        renderFb = ctx.device.createFramebufferUnique(fci);

        AssetID vsID = g_assetDB.addOrGetExisting("Shaders/standard.vert.spv");
        AssetID fsID = g_assetDB.addOrGetExisting("Shaders/standard.frag.spv");
        vertexShader = ShaderCache::getModule(ctx.device, vsID);
        fragmentShader = ShaderCache::getModule(ctx.device, fsID);

        auto msaaSamples = polyImage->image.info().samples;

        {
            AssetID vsID = g_assetDB.addOrGetExisting("Shaders/depth_prepass.vert.spv");
            AssetID fsID = g_assetDB.addOrGetExisting("Shaders/blank.frag.spv");
            auto preVertexShader = vku::loadShaderAsset(ctx.device, vsID);
            auto preFragmentShader = vku::loadShaderAsset(ctx.device, fsID);
            vku::PipelineMaker pm{ extent.width, extent.height };

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
            pmsci.rasterizationSamples = msaaSamples;
            pm.multisampleState(pmsci);
            pm.subPass(0);
            depthPrePipeline = pm.createUnique(ctx.device, ctx.pipelineCache, *pipelineLayout, *renderPass);
        }

        struct StandardSpecConsts {
            bool enablePicking = false;
            float parallaxMaxLayers = 32.0f;
            float parallaxMinLayers = 4.0f;
            bool doParallax = false;
        };

        // standard shader specialization constants
        vk::SpecializationMapEntry entries[4] = {
            { 0, offsetof(StandardSpecConsts, enablePicking), sizeof(bool) },
            { 1, offsetof(StandardSpecConsts, parallaxMaxLayers), sizeof(float) },
            { 2, offsetof(StandardSpecConsts, parallaxMinLayers), sizeof(float) },
            { 3, offsetof(StandardSpecConsts, doParallax), sizeof(bool) }
        };

        vk::SpecializationInfo standardSpecInfo { 4, entries, sizeof(StandardSpecConsts) };

        {
            vku::PipelineMaker pm{ extent.width, extent.height };

            StandardSpecConsts spc {
                enablePicking,
                maxParallaxLayers.getFloat(),
                minParallaxLayers.getFloat(),
                (bool)enableParallaxMapping.getInt()
            };

            standardSpecInfo.pData = &spc;

            pm.shader(vk::ShaderStageFlagBits::eFragment, fragmentShader, "main", &standardSpecInfo);
            pm.shader(vk::ShaderStageFlagBits::eVertex, vertexShader);
            pm.vertexBinding(0, (uint32_t)sizeof(Vertex));
            pm.vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, position));
            pm.vertexAttribute(1, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, normal));
            pm.vertexAttribute(2, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, tangent));
            pm.vertexAttribute(3, 0, vk::Format::eR32G32Sfloat, (uint32_t)offsetof(Vertex, uv));
            pm.cullMode(vk::CullModeFlagBits::eBack);

            if ((int)depthPrepass)
                pm.depthWriteEnable(false).depthTestEnable(true).depthCompareOp(vk::CompareOp::eEqual);
            else
                pm.depthWriteEnable(true).depthTestEnable(true).depthCompareOp(vk::CompareOp::eGreater);

            pm.blendBegin(false);
            pm.frontFace(vk::FrontFace::eCounterClockwise);
            pm.subPass(1);

            vk::PipelineMultisampleStateCreateInfo pmsci;
            pmsci.rasterizationSamples = msaaSamples;
            pm.multisampleState(pmsci);

            pipeline = pm.createUnique(ctx.device, ctx.pipelineCache, *pipelineLayout, *renderPass);
        }

        {
            AssetID fsID = g_assetDB.addOrGetExisting("Shaders/standard_alpha_test.frag.spv");
            auto atFragmentShader = vku::loadShaderAsset(ctx.device, fsID);

            vku::PipelineMaker pm{ extent.width, extent.height };

            // Sadly we can't enable picking for alpha test surfaces as we can't use
            // early fragment tests with them, which leads to strange issues.
            StandardSpecConsts spc {
                false,
                maxParallaxLayers.getFloat(),
                minParallaxLayers.getFloat(),
                (bool)enableParallaxMapping.getInt()
            };

            standardSpecInfo.pData = &spc;

            pm.shader(vk::ShaderStageFlagBits::eFragment, atFragmentShader, "main", &standardSpecInfo);
            pm.shader(vk::ShaderStageFlagBits::eVertex, vertexShader);
            pm.vertexBinding(0, (uint32_t)sizeof(Vertex));
            pm.vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, position));
            pm.vertexAttribute(1, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, normal));
            pm.vertexAttribute(2, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, tangent));
            pm.vertexAttribute(3, 0, vk::Format::eR32G32Sfloat, (uint32_t)offsetof(Vertex, uv));
            pm.cullMode(vk::CullModeFlagBits::eBack);
            pm.depthWriteEnable(true).depthTestEnable(true).depthCompareOp(vk::CompareOp::eGreater);

            pm.blendBegin(false);
            pm.frontFace(vk::FrontFace::eCounterClockwise);
            pm.subPass(1);

            vk::PipelineMultisampleStateCreateInfo pmsci;
            pmsci.rasterizationSamples = msaaSamples;
            pmsci.alphaToCoverageEnable = true;
            pm.multisampleState(pmsci);

            alphaTestPipeline = pm.createUnique(ctx.device, ctx.pipelineCache, *pipelineLayout, *renderPass);
        }

        {
            vku::PipelineMaker pm{ extent.width, extent.height };

            StandardSpecConsts spc {
                enablePicking,
                maxParallaxLayers.getFloat(),
                minParallaxLayers.getFloat(),
                (bool)enableParallaxMapping.getInt()
            };

            standardSpecInfo.pData = &spc;

            pm.shader(vk::ShaderStageFlagBits::eFragment, fragmentShader, "main", &standardSpecInfo);
            pm.shader(vk::ShaderStageFlagBits::eVertex, vertexShader);
            pm.vertexBinding(0, (uint32_t)sizeof(Vertex));
            pm.vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, position));
            pm.vertexAttribute(1, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, normal));
            pm.vertexAttribute(2, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, tangent));
            pm.vertexAttribute(3, 0, vk::Format::eR32G32Sfloat, (uint32_t)offsetof(Vertex, uv));
            pm.cullMode(vk::CullModeFlagBits::eNone);
            pm.depthWriteEnable(true).depthTestEnable(true).depthCompareOp(vk::CompareOp::eGreater);
            pm.blendBegin(false);
            pm.frontFace(vk::FrontFace::eCounterClockwise);
            pm.subPass(1);

            vk::PipelineMultisampleStateCreateInfo pmsci;
            pmsci.rasterizationSamples = msaaSamples;
            pmsci.alphaToCoverageEnable = true;
            pm.multisampleState(pmsci);
            noBackfaceCullPipeline = pm.createUnique(ctx.device, ctx.pipelineCache, *pipelineLayout, *renderPass);
        }

        {
            AssetID wvsID = g_assetDB.addOrGetExisting("Shaders/wire_obj.vert.spv");
            AssetID wfsID = g_assetDB.addOrGetExisting("Shaders/wire_obj.frag.spv");
            wireVertexShader = ShaderCache::getModule(ctx.device, wvsID);
            wireFragmentShader = ShaderCache::getModule(ctx.device, wfsID);

            vku::PipelineMaker pm{ extent.width, extent.height };
            pm.shader(vk::ShaderStageFlagBits::eFragment, wireFragmentShader);
            pm.shader(vk::ShaderStageFlagBits::eVertex, wireVertexShader);
            pm.depthWriteEnable(true).depthTestEnable(true).depthCompareOp(vk::CompareOp::eGreater);
            pm.vertexBinding(0, (uint32_t)sizeof(Vertex));
            pm.vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, position));
            pm.vertexAttribute(1, 0, vk::Format::eR32G32Sfloat, (uint32_t)offsetof(Vertex, uv));
            pm.polygonMode(vk::PolygonMode::eLine);
            pm.lineWidth(2.0f);
            pm.subPass(1);

            vk::PipelineMultisampleStateCreateInfo pmsci;
            pmsci.rasterizationSamples = msaaSamples;
            pm.multisampleState(pmsci);

            vku::PipelineLayoutMaker plm;
            plm.pushConstantRange(vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex, 0, sizeof(StandardPushConstants));
            plm.descriptorSetLayout(*dsl);
            wireframePipelineLayout = plm.createUnique(ctx.device);

            wireframePipeline = pm.createUnique(ctx.device, ctx.pipelineCache, *wireframePipelineLayout, *renderPass);
        }

        {
            currentLineVBSize = 0;

            vku::DescriptorSetLayoutMaker dslm;
            dslm.buffer(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex, 1);
            lineDsl = dslm.createUnique(ctx.device);

            vku::DescriptorSetMaker dsm;
            dsm.layout(*lineDsl);
            lineDs = std::move(dsm.createUnique(ctx.device, ctx.descriptorPool)[0]);

            vku::PipelineLayoutMaker linePl{};
            linePl.descriptorSetLayout(*lineDsl);
            linePipelineLayout = linePl.createUnique(ctx.device);

            vku::DescriptorSetUpdater dsu;
            dsu.beginDescriptorSet(*lineDs);
            dsu.beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer);
            dsu.buffer(vpUB.buffer(), 0, sizeof(MultiVP));
            dsu.update(ctx.device);

            vku::PipelineMaker pm{ extent.width, extent.height };
            AssetID vsID = g_assetDB.addOrGetExisting("Shaders/line.vert.spv");
            AssetID fsID = g_assetDB.addOrGetExisting("Shaders/line.frag.spv");

            auto vert = vku::loadShaderAsset(ctx.device, vsID);
            auto frag = vku::loadShaderAsset(ctx.device, fsID);

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

            vk::PipelineMultisampleStateCreateInfo pmsci;
            pmsci.rasterizationSamples = msaaSamples;
            pm.multisampleState(pmsci);

            linePipeline = pm.createUnique(ctx.device, ctx.pipelineCache, *linePipelineLayout, *renderPass);
        }

        {
            vku::DescriptorSetLayoutMaker dslm;
            dslm.buffer(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex, 1);
            dslm.image(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1);
            skyboxDsl = dslm.createUnique(ctx.device);

            vku::DescriptorSetMaker dsm;
            dsm.layout(*skyboxDsl);
            skyboxDs = std::move(dsm.createUnique(ctx.device, ctx.descriptorPool)[0]);

            vku::PipelineLayoutMaker skyboxPl{};
            skyboxPl.descriptorSetLayout(*skyboxDsl);
            skyboxPl.pushConstantRange(vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex, 0, sizeof(SkyboxPushConstants));
            skyboxPipelineLayout = skyboxPl.createUnique(ctx.device);

            vku::PipelineMaker pm{ extent.width, extent.height };
            AssetID vsID = g_assetDB.addOrGetExisting("Shaders/skybox.vert.spv");
            AssetID fsID = g_assetDB.addOrGetExisting("Shaders/skybox.frag.spv");

            auto vert = vku::loadShaderAsset(ctx.device, vsID);
            auto frag = vku::loadShaderAsset(ctx.device, fsID);

            pm.shader(vk::ShaderStageFlagBits::eFragment, frag);
            pm.shader(vk::ShaderStageFlagBits::eVertex, vert);
            pm.topology(vk::PrimitiveTopology::eTriangleList);
            pm.depthWriteEnable(true).depthTestEnable(true).depthCompareOp(vk::CompareOp::eGreaterOrEqual);

            vk::PipelineMultisampleStateCreateInfo pmsci;
            pmsci.rasterizationSamples = msaaSamples;
            pm.multisampleState(pmsci);
            pm.subPass(1);

            skyboxPipeline = pm.createUnique(ctx.device, ctx.pipelineCache, *skyboxPipelineLayout, *renderPass);
        }

        updateDescriptorSets(psCtx);

        if (ctx.graphicsSettings.enableVr) {
            cullMeshRenderer = new VRCullMeshRenderer{};
            cullMeshRenderer->setup(psCtx, *renderPass);
        }

        ctx.device.setEvent(*pickEvent);
    }

    struct SubmeshDrawInfo {
        uint32_t materialIdx;
        uint32_t matrixIdx;
        vk::Buffer vb;
        vk::Buffer ib;
        uint32_t indexCount;
        uint32_t indexOffset;
        uint32_t cubemapIdx;
        glm::vec3 cubemapExt;
        glm::vec3 cubemapPos;
        glm::vec4 texScaleOffset;
        entt::entity ent;
        vk::Pipeline pipeline;
        uint32_t drawMiscFlags;
        bool opaque;
    };

    slib::StaticAllocList<SubmeshDrawInfo> drawInfo{8192};

    void PolyRenderPass::prePass(PassSetupCtx& psCtx, RenderCtx& rCtx) {
        ZoneScoped;
        auto ctx = psCtx.vkCtx;

        Frustum frustum;
        Frustum frustumB;

        if (!rCtx.enableVR) {
            frustum.fromVPMatrix(rCtx.cam->getProjectionMatrix((float)rCtx.width / (float)rCtx.height) * rCtx.cam->getViewMatrix());
        } else {
            frustum.fromVPMatrix(rCtx.vrProjMats[0] * rCtx.vrViewMats[0]);
            frustumB.fromVPMatrix(rCtx.vrProjMats[1] * rCtx.vrViewMats[1]);
        }

        uint32_t skyboxId = rCtx.slotArrays.cubemaps.loadOrGet(rCtx.reg.ctx<SceneSettings>().skybox);
        if (skyboxId != lastSky) {
            dsUpdateNeeded = true;
            lastSky = skyboxId;
        }

        drawInfo.clear();

        int matrixIdx = 0;
        rCtx.reg.view<Transform, WorldObject>().each([&](entt::entity ent, Transform& t, WorldObject& wo) {
            if (matrixIdx == 1023) {
                fatalErr("Out of model matrices!");
                return;
            }

            auto meshPos = rCtx.loadedMeshes.find(wo.mesh);

            if (meshPos == rCtx.loadedMeshes.end()) {
                // Haven't loaded the mesh yet
                matrixIdx++;
                logWarn(WELogCategoryRender, "Missing mesh");
                return;
            }

            float maxScale = glm::max(t.scale.x, glm::max(t.scale.y, t.scale.z));
            if (!rCtx.enableVR) {
                if (!frustum.containsSphere(t.position, meshPos->second.sphereRadius * maxScale)) {
                    rCtx.dbgStats->numCulledObjs++;
                    return;
                }
            } else {
                if (!frustum.containsSphere(t.position, meshPos->second.sphereRadius * maxScale) &&
                    !frustumB.containsSphere(t.position, meshPos->second.sphereRadius * maxScale)) {
                    rCtx.dbgStats->numCulledObjs++;
                    return;
                }
            }

            modelMatricesMapped->modelMatrices[matrixIdx] = t.getMatrix();

            for (int i = 0; i < meshPos->second.numSubmeshes; i++) {
                auto& currSubmesh = meshPos->second.submeshes[i];

                SubmeshDrawInfo sdi;
                sdi.ib = meshPos->second.ib.buffer();
                sdi.vb = meshPos->second.vb.buffer();
                sdi.indexCount = currSubmesh.indexCount;
                sdi.indexOffset = currSubmesh.indexOffset;
                sdi.materialIdx = wo.materialIdx[i];
                sdi.matrixIdx = matrixIdx;
                sdi.texScaleOffset = wo.texScaleOffset;
                sdi.ent = ent;
                auto& packedMat = rCtx.slotArrays.materials[wo.materialIdx[i]];
                sdi.opaque = packedMat.getCutoff() == 0.0f;

                switch (wo.uvOverride) {
                default:
                    sdi.drawMiscFlags = 0;
                    break;
                case UVOverride::XY:
                    sdi.drawMiscFlags = 128;
                    break;
                case UVOverride::XZ:
                    sdi.drawMiscFlags = 256;
                    break;
                case UVOverride::ZY:
                    sdi.drawMiscFlags = 512;
                    break;
                case UVOverride::PickBest:
                    sdi.drawMiscFlags = 1024;
                    break;
                }

                uint32_t currCubemapIdx = skyboxId;

                rCtx.reg.view<WorldCubemap, Transform>().each([&](auto, WorldCubemap& wc, Transform& cubeT) {
                    glm::vec3 cPos = t.position;
                    glm::vec3 ma = wc.extent + cubeT.position;
                    glm::vec3 mi = cubeT.position - wc.extent;

                    if (cPos.x < ma.x && cPos.x > mi.x &&
                        cPos.y < ma.y && cPos.y > mi.y &&
                        cPos.z < ma.z && cPos.z > mi.z) {
                        currCubemapIdx = rCtx.slotArrays.cubemaps.get(wc.cubemapId);
                        if (wc.cubeParallax) {
                            sdi.drawMiscFlags |= 4096; // flag for cubemap parallax correction
                            sdi.cubemapPos = cubeT.position;
                            sdi.cubemapExt = wc.extent;
                        }
                    }
                });

                sdi.cubemapIdx = currCubemapIdx;

                auto& extraDat = rCtx.slotArrays.materials.getExtraDat(wo.materialIdx[i]);

                if (extraDat.noCull) {
                    sdi.pipeline = *noBackfaceCullPipeline;
                } else if (extraDat.wireframe || showWireframe.getInt() == 1) {
                    sdi.pipeline = *wireframePipeline;
                } else if (rCtx.reg.has<UseWireframe>(ent) || showWireframe.getInt() == 2) {
                    if (sdi.opaque) {
                        sdi.pipeline = *pipeline;
                    } else {
                        sdi.pipeline = *alphaTestPipeline;
                    }

                    drawInfo.add(sdi);
                    sdi.pipeline = *wireframePipeline;
                } else {
                    if (sdi.opaque) {
                        sdi.pipeline = *pipeline;
                    } else {
                        sdi.pipeline = *alphaTestPipeline;
                    }
                }
                rCtx.dbgStats->numTriangles += currSubmesh.indexCount / 3;

                drawInfo.add(std::move(sdi));
            }

            matrixIdx++;
        });

        rCtx.reg.view<Transform, ProceduralObject>().each([&](auto ent, Transform& t, ProceduralObject& po) {
            if (matrixIdx == 1023) {
                fatalErr("Out of model matrices!");
                return;
            }

            glm::mat4 m = t.getMatrix();
            modelMatricesMapped->modelMatrices[matrixIdx] = m;
            matrixIdx++;
        });

        if (rCtx.enableVR) {
            vpMapped->views[0] = rCtx.vrViewMats[0];
            vpMapped->views[1] = rCtx.vrViewMats[1];
            vpMapped->projections[0] = rCtx.vrProjMats[0];
            vpMapped->projections[1] = rCtx.vrProjMats[1];
        } else {
            vpMapped->views[0] = rCtx.cam->getViewMatrix();
            vpMapped->projections[0] = rCtx.cam->getProjectionMatrix((float)rCtx.width / (float)rCtx.height);
            vpMapped->viewPos[0] = glm::vec4(rCtx.cam->position, 0.0f);
        }

        int lightIdx = 0;
        rCtx.reg.view<WorldLight, Transform>().each([&](auto ent, WorldLight& l, Transform& transform) {
            if (!l.enabled) return;
            glm::vec3 lightForward = glm::normalize(transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f));
            if (l.type != LightType::Tube) {
                lightMapped->lights[lightIdx] = PackedLight{
                    glm::vec4(l.color * l.intensity, (float)l.type),
                    glm::vec4(lightForward, l.spotCutoff),
                    glm::vec4(transform.position, 0.0f)
                };
            } else {
                glm::vec3 tubeP0 = transform.position + lightForward * l.tubeLength;
                glm::vec3 tubeP1 = transform.position - lightForward * l.tubeLength;
                lightMapped->lights[lightIdx] = PackedLight{
                    glm::vec4(l.color * l.intensity, (float)l.type),
                    glm::vec4(tubeP0, l.tubeRadius),
                    glm::vec4(tubeP1, 0.0f)
                };
            }
            lightIdx++;
        });

        lightMapped->pack0.x = (float)lightIdx;
        lightMapped->pack0.y = rCtx.cascadeTexelsPerUnit[0];
        lightMapped->pack0.z = rCtx.cascadeTexelsPerUnit[1];
        lightMapped->pack0.w = rCtx.cascadeTexelsPerUnit[2];
        lightMapped->shadowmapMatrices[0] = rCtx.cascadeShadowMatrices[0];
        lightMapped->shadowmapMatrices[1] = rCtx.cascadeShadowMatrices[1];
        lightMapped->shadowmapMatrices[2] = rCtx.cascadeShadowMatrices[2];

        uint32_t aoBoxIdx = 0;
        rCtx.reg.view<Transform, ProxyAOComponent>().each([&](auto ent, Transform& t, ProxyAOComponent& pac) {
            lightMapped->box[aoBoxIdx].setScale(pac.bounds);
            glm::mat4 tMat = glm::translate(glm::mat4(1.0f), t.position);
            lightMapped->box[aoBoxIdx].setMatrix(glm::mat4_cast(glm::inverse(t.rotation)) * glm::inverse(tMat));
            lightMapped->box[aoBoxIdx].setEntityId((uint32_t)ent);
            aoBoxIdx++;
        });
        lightMapped->pack1.x = aoBoxIdx;

        if (dsUpdateNeeded) {
            // Update descriptor sets to bring in any new textures
            updateDescriptorSets(psCtx);
        }

        auto& renderBuffer = g_scene->getRenderBuffer();
        uint32_t requiredVBSize = renderBuffer.getNbLines() * 2u;

        if (!lineVB.buffer() || currentLineVBSize < requiredVBSize) {
            currentLineVBSize = requiredVBSize + 128;
            lineVB = vku::GenericBuffer{ ctx.device, ctx.allocator, vk::BufferUsageFlagBits::eVertexBuffer, sizeof(LineVert) * currentLineVBSize, VMA_MEMORY_USAGE_CPU_TO_GPU, "Line Buffer" };
        }

        if (currentLineVBSize > 0) {
            LineVert* lineVBDat = (LineVert*)lineVB.map(ctx.device);
            for (uint32_t i = 0; i < renderBuffer.getNbLines(); i++) {
                const auto& physLine = renderBuffer.getLines()[i];
                lineVBDat[(i * 2) + 0] = LineVert{ px2glm(physLine.pos0), glm::vec4(1.0f, 0.0f, 1.0f, 1.0f) };
                lineVBDat[(i * 2) + 1] = LineVert{ px2glm(physLine.pos1), glm::vec4(1.0f, 0.0f, 1.0f, 1.0f) };
            }

            lineVB.unmap(ctx.device);
            lineVB.invalidate(ctx.device);
            lineVB.flush(ctx.device);
            numLineVerts = renderBuffer.getNbLines() * 2;
        }
    }

    void PolyRenderPass::execute(RenderCtx& ctx) {
        ZoneScoped;
        TracyVkZone((*ctx.tracyContexts)[ctx.imageIndex], *ctx.cmdBuf, "Polys");

        std::array<float, 4> clearColorValue{ 0.0f, 0.0f, 0.0f, 1 };
        vk::ClearDepthStencilValue clearDepthValue{ 0.0f, 0 };
        std::array<vk::ClearValue, 2> clearColours{ vk::ClearValue(clearColorValue), clearDepthValue };
        vk::RenderPassBeginInfo rpbi;

        rpbi.renderPass = *renderPass;
        rpbi.framebuffer = *renderFb;
        rpbi.renderArea = vk::Rect2D{ {0, 0}, {ctx.width, ctx.height} };
        rpbi.clearValueCount = (uint32_t)clearColours.size();
        rpbi.pClearValues = clearColours.data();

        vk::CommandBuffer cmdBuf = ctx.cmdBuf;

        vpUB.barrier(
            cmdBuf, vk::PipelineStageFlagBits::eHost, vk::PipelineStageFlagBits::eVertexShader,
            vk::DependencyFlagBits::eByRegion, vk::AccessFlagBits::eHostWrite, vk::AccessFlagBits::eUniformRead,
            VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED);

        lightsUB.barrier(
            cmdBuf, vk::PipelineStageFlagBits::eHost, vk::PipelineStageFlagBits::eFragmentShader,
            vk::DependencyFlagBits::eByRegion, vk::AccessFlagBits::eHostWrite, vk::AccessFlagBits::eUniformRead,
            VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED);

        if (pickThisFrame) {
            pickingBuffer.barrier(cmdBuf, vk::PipelineStageFlagBits::eHost, vk::PipelineStageFlagBits::eTransfer,
                vk::DependencyFlagBits::eByRegion,
                vk::AccessFlagBits::eHostRead, vk::AccessFlagBits::eTransferWrite,
                VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED);

            PickingBuffer pb;
            pb.objectID = ~0u;
            cmdBuf.updateBuffer(pickingBuffer.buffer(), 0, sizeof(pb), &pb);

            pickingBuffer.barrier(cmdBuf, vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
                vk::DependencyFlagBits::eByRegion,
                vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead,
                VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED);
        }

        if (setEventNextFrame) {
            cmdBuf.setEvent(*pickEvent, vk::PipelineStageFlagBits::eAllCommands);
            setEventNextFrame = false;
        }

        cmdBuf.beginRenderPass(rpbi, vk::SubpassContents::eInline);

        if (ctx.enableVR) {
            cullMeshRenderer->draw(cmdBuf);
        }

        cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, *descriptorSet, nullptr);

        uint32_t globalMiscFlags = 0;

        if (pickThisFrame)
            globalMiscFlags |= 1;

        if (dbgDrawMode.getInt() != 0) {
            globalMiscFlags |= (1 << dbgDrawMode.getInt());
        }

        if (!ctx.enableShadows) {
            globalMiscFlags |= 16384;
        }

        //std::sort(drawInfo.begin(), drawInfo.end(), [&](const auto& sdiA, const auto& sdiB) {
        //    return sdiA.pipeline > sdiB.pipeline;
        //});

        if ((int)depthPrepass) {
            ZoneScopedN("Depth prepass");
            cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, *depthPrePipeline);

            for (auto& sdi : drawInfo) {
                if (sdi.pipeline != *pipeline || !sdi.opaque) {
                    continue;
                }

                StandardPushConstants pushConst {
                    .modelMatrixIdx = sdi.matrixIdx,
                    .materialIdx = sdi.materialIdx,
                    .vpIdx = 0,
                    .objectId = (uint32_t)sdi.ent,
                    .texScaleOffset = sdi.texScaleOffset,
                    .screenSpacePickPos = glm::ivec3(pickX, pickY, globalMiscFlags | sdi.drawMiscFlags)
                };
                cmdBuf.pushConstants<StandardPushConstants>(*pipelineLayout, vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex, 0, pushConst);
                cmdBuf.bindVertexBuffers(0, sdi.vb, vk::DeviceSize(0));
                cmdBuf.bindIndexBuffer(sdi.ib, 0, vk::IndexType::eUint32);
                cmdBuf.drawIndexed(sdi.indexCount, 1, sdi.indexOffset, 0, 0);
                ctx.dbgStats->numDrawCalls++;
            }
        }

        cmdBuf.nextSubpass(vk::SubpassContents::eInline);

        cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
        SubmeshDrawInfo last;
        last.pipeline = *pipeline;
        for (const auto& sdi : drawInfo) {
            ZoneScopedN("SDI cmdbuf write");

            if (last.pipeline != sdi.pipeline) {
                cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, sdi.pipeline);
                ctx.dbgStats->numPipelineSwitches++;
            }

            StandardPushConstants pushConst {
                .modelMatrixIdx = sdi.matrixIdx,
                .materialIdx = sdi.materialIdx,
                .vpIdx = 0,
                .objectId = (uint32_t)sdi.ent,
                .cubemapExt = glm::vec4(sdi.cubemapExt, 0.0f),
                .cubemapPos = glm::vec4(sdi.cubemapPos, 0.0f),
                .texScaleOffset = sdi.texScaleOffset,
                .screenSpacePickPos = glm::ivec3(pickX, pickY, globalMiscFlags | sdi.drawMiscFlags),
                .cubemapIdx = sdi.cubemapIdx
            };

            cmdBuf.pushConstants<StandardPushConstants>(*pipelineLayout, vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex, 0, pushConst);
            cmdBuf.bindVertexBuffers(0, sdi.vb, vk::DeviceSize(0));
            cmdBuf.bindIndexBuffer(sdi.ib, 0, vk::IndexType::eUint32);
            cmdBuf.drawIndexed(sdi.indexCount, 1, sdi.indexOffset, 0, 0);

            last = sdi;
            ctx.dbgStats->numDrawCalls++;
        }

        if (numLineVerts > 0) {
            cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, *linePipeline);
            cmdBuf.bindVertexBuffers(0, lineVB.buffer(), vk::DeviceSize(0));
            cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *linePipelineLayout, 0, *lineDs, nullptr);
            cmdBuf.draw(numLineVerts, 1, 0, 0);
            ctx.dbgStats->numDrawCalls++;
        }

        cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, *skyboxPipeline);
        cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *skyboxPipelineLayout, 0, *skyboxDs, nullptr);
        SkyboxPushConstants spc{ glm::ivec4(0, 0, 0, 0) };
        cmdBuf.pushConstants<SkyboxPushConstants>(*skyboxPipelineLayout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, spc);
        cmdBuf.draw(36, 1, 0, 0);
        ctx.dbgStats->numDrawCalls++;

        cmdBuf.endRenderPass();
        polyImage->image.setCurrentLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        depthStencilImage->image.setCurrentLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

        if (pickThisFrame) {
            cmdBuf.resetEvent(*pickEvent, vk::PipelineStageFlagBits::eBottomOfPipe);
            pickThisFrame = false;
        }
    }

    void PolyRenderPass::requestEntityPick() {
        if (awaitingResults) return;
        pickThisFrame = true;
        awaitingResults = true;
    }

    bool PolyRenderPass::getPickedEnt(uint32_t* entOut) {
        auto device = pickEvent.getOwner(); // bleh
        vk::Result pickEvtRes = pickEvent.getOwner().getEventStatus(*pickEvent);

        if (pickEvtRes != vk::Result::eEventReset)
            return false;

        PickingBuffer* pickBuf = (PickingBuffer*)pickingBuffer.map(device);
        *entOut = pickBuf->objectID;

        pickingBuffer.unmap(device);

        setEventNextFrame = true;
        awaitingResults = false;

        return true;
    }

    void PolyRenderPass::lateUpdateVP(glm::mat4 views[2], glm::vec3 viewPos[2], vk::Device dev) {
        vpMapped->views[0] = views[0];
        vpMapped->views[1] = views[1];
        vpMapped->viewPos[0] = glm::vec4(viewPos[0], 0.0f);
        vpMapped->viewPos[1] = glm::vec4(viewPos[1], 0.0f);
    }

    PolyRenderPass::~PolyRenderPass() {
        // BLEUGHHH
        vk::Device device = pipeline.getOwner();
        modelMatrixUB.unmap(device);
        lightsUB.unmap(device);
        vpUB.unmap(device);
        lineVB.destroy();
    }
}
