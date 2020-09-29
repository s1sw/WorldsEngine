#include "RenderPasses.hpp"
#include "Engine.hpp"
#include "Transform.hpp"
#include "spirv_reflect.h"
#include <openvr.h>
#include "tracy/Tracy.hpp"
#include "Render.hpp"
#include "Physics.hpp"
#include "Frustum.hpp"

namespace worlds {
    struct StandardPushConstants {
        glm::vec4 texScaleOffset;
        // (x: model matrix index, y: material index, z: specular cubemap index, w: object picking id)
        glm::ivec4 ubIndices;
        glm::ivec4 screenSpacePickPos;
    };

    struct SkyboxPushConstants {
        // (x: vp index, y: cubemap index)
        glm::ivec4 ubIndices;
    };

    struct PickingBuffer {
        uint32_t depth;
        uint32_t objectID;
        uint32_t lock;
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
        {
            vku::DescriptorSetUpdater updater(10, 128, 0);
            updater.beginDescriptorSet(*descriptorSet);

            updater.beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer);
            updater.buffer(this->vpUB.buffer(), 0, sizeof(MultiVP));

            updater.beginBuffers(1, 0, vk::DescriptorType::eUniformBuffer);
            updater.buffer(lightsUB.buffer(), 0, sizeof(LightUB));

            updater.beginBuffers(2, 0, vk::DescriptorType::eUniformBuffer);
            updater.buffer(materialUB.buffer(), 0, sizeof(MaterialsUB));

            updater.beginBuffers(3, 0, vk::DescriptorType::eUniformBuffer);
            updater.buffer(modelMatrixUB.buffer(), 0, sizeof(ModelMatrices));

            for (uint32_t i = 0; i < ctx.globalTexArray->get()->size(); i++) {
                if ((*ctx.globalTexArray)->isSlotPresent(i)) {
                    updater.beginImages(4, i, vk::DescriptorType::eCombinedImageSampler);
                    updater.image(*albedoSampler, (*(*ctx.globalTexArray))[i].imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
                }
            }

            updater.beginImages(5, 0, vk::DescriptorType::eCombinedImageSampler);
            updater.image(*shadowSampler, ctx.rtResources.at(shadowImage).image.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

            for (uint32_t i = 0; i < ctx.cubemapSlots->get()->size(); i++) {
                if ((*ctx.cubemapSlots)->isSlotPresent(i)) {
                    updater.beginImages(6, i, vk::DescriptorType::eCombinedImageSampler);
                    updater.image(*albedoSampler, (*(*ctx.cubemapSlots))[i].imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
                }
            }

            updater.beginImages(7, 0, vk::DescriptorType::eCombinedImageSampler);
            updater.image(*albedoSampler, ctx.brdfLut->imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

            updater.beginBuffers(8, 0, vk::DescriptorType::eStorageBuffer);
            updater.buffer(pickingBuffer.buffer(), 0, sizeof(PickingBuffer));

            if (!updater.ok())
                __debugbreak();

            updater.update(ctx.device);
        }

        {
            vku::DescriptorSetUpdater updater;
            updater.beginDescriptorSet(*skyboxDs);
            updater.beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer);
            updater.buffer(vpUB.buffer(), 0, sizeof(MultiVP));

            for (uint32_t i = 0; i < ctx.cubemapSlots->get()->size(); i++) {
                if ((*ctx.cubemapSlots)->isSlotPresent(i)) {
                    updater.beginImages(1, i, vk::DescriptorType::eCombinedImageSampler);
                    updater.image(*albedoSampler, (*(*ctx.cubemapSlots))[i].imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
                }
            }

            updater.update(ctx.device);
        }
    }

    PolyRenderPass::PolyRenderPass(
        RenderImageHandle depthStencilImage,
        RenderImageHandle polyImage,
        RenderImageHandle shadowImage,
        bool enablePicking)
        : depthStencilImage(depthStencilImage)
        , polyImage(polyImage)
        , shadowImage(shadowImage)
        , enablePicking(enablePicking)
        , pickX(0)
        , pickY(0)
        , pickedEnt(UINT32_MAX)
        , awaitingResults(false)
        , pickThisFrame(false)
        , setEventNextFrame(false) {

    }

    RenderPassIO PolyRenderPass::getIO() {
        RenderPassIO io;
        io.inputs = {
            {
                vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::PipelineStageFlagBits::eFragmentShader,
                vk::AccessFlagBits::eShaderRead,
                shadowImage
            }
        };

        io.outputs = {
            {
                vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::PipelineStageFlagBits::eColorAttachmentOutput,
                vk::AccessFlagBits::eColorAttachmentWrite,
                polyImage
            }
        };
        return io;
    }

    void PolyRenderPass::setup(PassSetupCtx& ctx) {
        ZoneScoped;
        auto memoryProps = ctx.physicalDevice.getMemoryProperties();

        vku::SamplerMaker sm{};
        sm.magFilter(vk::Filter::eLinear).minFilter(vk::Filter::eLinear).mipmapMode(vk::SamplerMipmapMode::eLinear).anisotropyEnable(true).maxAnisotropy(16.0f).maxLod(100.0f).minLod(0.0f);
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
        dslm.buffer(2, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eFragment, 1);
        // Model matrices
        dslm.buffer(3, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex, 1);
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

        this->dsl = dslm.createUnique(ctx.device);

        vku::PipelineLayoutMaker plm;
        plm.pushConstantRange(vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex, 0, sizeof(StandardPushConstants));
        plm.descriptorSetLayout(*this->dsl);
        this->pipelineLayout = plm.createUnique(ctx.device);

        this->vpUB = vku::UniformBuffer(ctx.device, ctx.allocator, sizeof(MultiVP), VMA_MEMORY_USAGE_CPU_TO_GPU, "VP");
        lightsUB = vku::UniformBuffer(ctx.device, ctx.allocator, sizeof(LightUB), VMA_MEMORY_USAGE_CPU_TO_GPU, "Lights");
        materialUB = vku::UniformBuffer(ctx.device, ctx.allocator, sizeof(MaterialsUB), VMA_MEMORY_USAGE_GPU_ONLY, "Materials");
        modelMatrixUB = vku::UniformBuffer(ctx.device, ctx.allocator, sizeof(ModelMatrices), VMA_MEMORY_USAGE_CPU_TO_GPU, "Model matrices");
        pickingBuffer = vku::GenericBuffer(ctx.device, ctx.allocator, vk::BufferUsageFlagBits::eStorageBuffer, sizeof(PickingBuffer), VMA_MEMORY_USAGE_GPU_TO_CPU, "Picking buffer");

        pickEvent = ctx.device.createEventUnique(vk::EventCreateInfo{});

        MaterialsUB materials;
        //materials.materials[0] = { glm::vec4(0.0f, 0.02f, 0.0f, 0.0f), glm::vec4(1.0f, 1.0f, 1.0f, 0.0f) };
        //materials.materials[1] = { glm::vec4(0.0f, 0.2f, 1.0f, 0.0f), glm::vec4(1.0f, 1.0f, 1.0f, 0.0f) };
        materialUB.upload(ctx.device, memoryProps, ctx.commandPool, ctx.device.getQueue(ctx.graphicsQueueFamilyIdx, 0), materials);

        vku::DescriptorSetMaker dsm;
        dsm.layout(*this->dsl);
        descriptorSet = std::move(dsm.createUnique(ctx.device, ctx.descriptorPool)[0]);

        vku::RenderpassMaker rPassMaker;

        rPassMaker.attachmentBegin(vk::Format::eR16G16B16A16Sfloat);
        rPassMaker.attachmentLoadOp(vk::AttachmentLoadOp::eClear);
        rPassMaker.attachmentStoreOp(vk::AttachmentStoreOp::eStore);
        rPassMaker.attachmentSamples(vku::sampleCountFlags(ctx.graphicsSettings.msaaLevel));
        rPassMaker.attachmentFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

        rPassMaker.attachmentBegin(vk::Format::eD32Sfloat);
        rPassMaker.attachmentLoadOp(vk::AttachmentLoadOp::eClear);
        rPassMaker.attachmentStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
        rPassMaker.attachmentSamples(vku::sampleCountFlags(ctx.graphicsSettings.msaaLevel));
        rPassMaker.attachmentFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

        rPassMaker.subpassBegin(vk::PipelineBindPoint::eGraphics);
        rPassMaker.subpassColorAttachment(vk::ImageLayout::eColorAttachmentOptimal, 0);
        rPassMaker.subpassDepthStencilAttachment(vk::ImageLayout::eDepthStencilAttachmentOptimal, 1);

        rPassMaker.dependencyBegin(VK_SUBPASS_EXTERNAL, 0);
        rPassMaker.dependencySrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
        rPassMaker.dependencyDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
        rPassMaker.dependencyDstAccessMask(vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite);

        vk::RenderPassMultiviewCreateInfo renderPassMultiviewCI{};
        /*
                Bit mask that specifies which view rendering is broadcast to
                0011 = Broadcast to first and second view (layer)
            */
        const uint32_t viewMask = 0b00000011;

        /*
            Bit mask that specifices correlation between views
            An implementation may use this for optimizations (concurrent render)
        */
        const uint32_t correlationMask = 0b00000011;

        renderPassMultiviewCI.subpassCount = 1;
        renderPassMultiviewCI.pViewMasks = &viewMask;
        renderPassMultiviewCI.correlationMaskCount = 1;
        renderPassMultiviewCI.pCorrelationMasks = &correlationMask;

        if (ctx.graphicsSettings.enableVr) {
            rPassMaker.setPNext(&renderPassMultiviewCI);
        }

        this->renderPass = rPassMaker.createUnique(ctx.device);

        vk::ImageView attachments[2] = { ctx.rtResources.at(polyImage).image.imageView(), ctx.rtResources.at(depthStencilImage).image.imageView() };

        auto extent = ctx.rtResources.at(polyImage).image.info().extent;
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
        vertexShader = vku::loadShaderAsset(ctx.device, vsID);
        fragmentShader = vku::loadShaderAsset(ctx.device, fsID);
        
        {
            vku::PipelineMaker pm{ extent.width, extent.height };

            vk::SpecializationMapEntry pickingEntry{ 0, 0, sizeof(bool) };
            vk::SpecializationInfo si;
            si.dataSize = sizeof(bool);
            si.mapEntryCount = 1;
            si.pMapEntries = &pickingEntry;
            si.pData = &enablePicking;

            pm.shader(vk::ShaderStageFlagBits::eFragment, fragmentShader, "main", &si);
            pm.shader(vk::ShaderStageFlagBits::eVertex, vertexShader);
            pm.vertexBinding(0, (uint32_t)sizeof(Vertex));
            pm.vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, position));
            pm.vertexAttribute(1, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, normal));
            pm.vertexAttribute(2, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, tangent));
            pm.vertexAttribute(3, 0, vk::Format::eR32G32Sfloat, (uint32_t)offsetof(Vertex, uv));
            pm.cullMode(vk::CullModeFlagBits::eBack);
            pm.depthWriteEnable(true).depthTestEnable(true).depthCompareOp(vk::CompareOp::eLess);
            pm.blendBegin(false);
            pm.frontFace(vk::FrontFace::eCounterClockwise);

            vk::PipelineMultisampleStateCreateInfo pmsci;
            pmsci.rasterizationSamples = (vk::SampleCountFlagBits)ctx.graphicsSettings.msaaLevel;
            pmsci.alphaToCoverageEnable = true;
            pm.multisampleState(pmsci);
            pipeline = pm.createUnique(ctx.device, ctx.pipelineCache, *pipelineLayout, *renderPass);
        }

        {
            vku::PipelineMaker pm{ extent.width, extent.height };

            vk::SpecializationMapEntry pickingEntry{ 0, 0, sizeof(bool) };
            vk::SpecializationInfo si;
            si.dataSize = sizeof(bool);
            si.mapEntryCount = 1;
            si.pMapEntries = &pickingEntry;
            si.pData = &enablePicking;

            pm.shader(vk::ShaderStageFlagBits::eFragment, fragmentShader, "main", &si);
            pm.shader(vk::ShaderStageFlagBits::eVertex, vertexShader);
            pm.vertexBinding(0, (uint32_t)sizeof(Vertex));
            pm.vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, position));
            pm.vertexAttribute(1, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, normal));
            pm.vertexAttribute(2, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, tangent));
            pm.vertexAttribute(3, 0, vk::Format::eR32G32Sfloat, (uint32_t)offsetof(Vertex, uv));
            pm.cullMode(vk::CullModeFlagBits::eNone);
            pm.depthWriteEnable(true).depthTestEnable(true).depthCompareOp(vk::CompareOp::eLess);
            pm.blendBegin(false);
            pm.frontFace(vk::FrontFace::eCounterClockwise);

            vk::PipelineMultisampleStateCreateInfo pmsci;
            pmsci.rasterizationSamples = (vk::SampleCountFlagBits)ctx.graphicsSettings.msaaLevel;
            pmsci.alphaToCoverageEnable = true;
            pm.multisampleState(pmsci);
            noBackfaceCullPipeline = pm.createUnique(ctx.device, ctx.pipelineCache, *pipelineLayout, *renderPass);
        }

        {
            AssetID wvsID = g_assetDB.addOrGetExisting("Shaders/wire_obj.vert.spv");
            AssetID wfsID = g_assetDB.addOrGetExisting("Shaders/wire_obj.frag.spv");
            wireVertexShader = vku::loadShaderAsset(ctx.device, wvsID);
            wireFragmentShader = vku::loadShaderAsset(ctx.device, wfsID);

            vku::PipelineMaker pm{ extent.width, extent.height };
            pm.shader(vk::ShaderStageFlagBits::eFragment, wireFragmentShader);
            pm.shader(vk::ShaderStageFlagBits::eVertex, wireVertexShader);
            pm.depthWriteEnable(true).depthTestEnable(true).depthCompareOp(vk::CompareOp::eLess);
            pm.vertexBinding(0, (uint32_t)sizeof(Vertex));
            pm.vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, position));
            pm.vertexAttribute(1, 0, vk::Format::eR32G32Sfloat, (uint32_t)offsetof(Vertex, uv));
            pm.polygonMode(vk::PolygonMode::eLine);
            pm.lineWidth(2.0f);

            vk::PipelineMultisampleStateCreateInfo pmsci;
            pmsci.rasterizationSamples = (vk::SampleCountFlagBits)ctx.graphicsSettings.msaaLevel;
            pm.multisampleState(pmsci);

            vku::PipelineLayoutMaker plm;
            plm.pushConstantRange(vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex, 0, sizeof(StandardPushConstants));
            plm.descriptorSetLayout(*dsl);
            wireframePipelineLayout = plm.createUnique(ctx.device);

            wireframePipeline = pm.createUnique(ctx.device, ctx.pipelineCache, *wireframePipelineLayout, *renderPass);
        }

        {
            vku::DescriptorSetLayoutMaker cDslm{};
            cDslm.buffer(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute, 1);
            pickingBufCsDsl = cDslm.createUnique(ctx.device);

            vku::PipelineLayoutMaker cPlm{};
            cPlm.descriptorSetLayout(*pickingBufCsDsl);
            cPlm.pushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(PickBufCSPushConstants));
            pickingBufCsLayout = cPlm.createUnique(ctx.device);

            vku::ComputePipelineMaker cpm{};
            vku::ShaderModule sm = vku::loadShaderAsset(ctx.device, g_assetDB.addOrGetExisting("Shaders/clear_pick_buf.comp.spv"));
            cpm.shader(vk::ShaderStageFlagBits::eCompute, sm);
            pickingBufCsPipeline = cpm.createUnique(ctx.device, ctx.pipelineCache, *pickingBufCsLayout);

            vku::DescriptorSetMaker dsm{};
            dsm.layout(*pickingBufCsDsl);
            pickingBufCsDs = std::move(dsm.createUnique(ctx.device, ctx.descriptorPool)[0]);

            vku::DescriptorSetUpdater dsu{};
            dsu.beginDescriptorSet(*pickingBufCsDs);
            dsu.beginBuffers(0, 0, vk::DescriptorType::eStorageBuffer);
            dsu.buffer(pickingBuffer.buffer(), 0, sizeof(PickingBuffer));
            dsu.update(ctx.device);
        }

        {
            /*lineVB = vku::GenericBuffer{ ctx.device, ctx.allocator, vk::BufferUsageFlagBits::eVertexBuffer, sizeof(LineVert) * 256, VMA_MEMORY_USAGE_CPU_TO_GPU, "Line Buffer" };*/
            currentLineVBSize = 0;

            vku::DescriptorSetLayoutMaker dslm;
            dslm.buffer(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex, 1);
            lineDsl = dslm.createUnique(ctx.device);

            vku::DescriptorSetMaker dsm;
            dsm.layout(*lineDsl);
            lineDs = dsm.create(ctx.device, ctx.descriptorPool)[0];

            vku::PipelineLayoutMaker linePl{};
            linePl.descriptorSetLayout(*lineDsl);
            linePipelineLayout = linePl.createUnique(ctx.device);

            vku::DescriptorSetUpdater dsu;
            dsu.beginDescriptorSet(lineDs);
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
            pm.depthWriteEnable(true).depthTestEnable(true).depthCompareOp(vk::CompareOp::eLess);

            vk::PipelineMultisampleStateCreateInfo pmsci;
            pmsci.rasterizationSamples = (vk::SampleCountFlagBits)ctx.graphicsSettings.msaaLevel;
            pm.multisampleState(pmsci);

            linePipeline = pm.createUnique(ctx.device, ctx.pipelineCache, *linePipelineLayout, *renderPass);
        }

        {
            vku::DescriptorSetLayoutMaker dslm;
            dslm.buffer(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex, 1);
            dslm.image(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, NUM_CUBEMAP_SLOTS);
            dslm.bindFlag(1, vk::DescriptorBindingFlagBits::ePartiallyBound);
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
            pm.depthWriteEnable(true).depthTestEnable(true).depthCompareOp(vk::CompareOp::eLessOrEqual);

            vk::PipelineMultisampleStateCreateInfo pmsci;
            pmsci.rasterizationSamples = (vk::SampleCountFlagBits)ctx.graphicsSettings.msaaLevel;
            pm.multisampleState(pmsci);

            skyboxPipeline = pm.createUnique(ctx.device, ctx.pipelineCache, *skyboxPipelineLayout, *renderPass);
        }

        materialUB.upload(ctx.device, memoryProps, ctx.commandPool, ctx.device.getQueue(ctx.graphicsQueueFamilyIdx, 0), (*ctx.materialSlots)->getSlots(), sizeof(PackedMaterial) * 256);

        updateDescriptorSets(ctx);

        ctx.device.setEvent(*pickEvent);
    }

    void PolyRenderPass::prePass(PassSetupCtx& ctx, RenderCtx& rCtx) {
        ZoneScoped;
        auto tfWoView = rCtx.reg.view<Transform, WorldObject>();

        ModelMatrices* modelMatricesMapped = static_cast<ModelMatrices*>(modelMatrixUB.map(ctx.device));

        int matrixIdx = 0;
        rCtx.reg.view<Transform, WorldObject>().each([&matrixIdx, modelMatricesMapped](auto ent, Transform& t, WorldObject& wo) {
            if (matrixIdx == 1023) {
                fatalErr("Out of model matrices! Either don't spam so many objects or shout at us on the bug tracker.");
                return;
            }
            glm::mat4 m = t.getMatrix();
            modelMatricesMapped->modelMatrices[matrixIdx] = m;
            matrixIdx++;
            });

        rCtx.reg.view<Transform, ProceduralObject>().each([&matrixIdx, modelMatricesMapped](auto ent, Transform& t, ProceduralObject& po) {
            if (matrixIdx == 1023) {
                fatalErr("Out of model matrices! Either don't spam so many objects or shout at us on the bug tracker.");
                return;
            }
            glm::mat4 m = t.getMatrix();
            modelMatricesMapped->modelMatrices[matrixIdx] = m;
            matrixIdx++;
            });

        modelMatrixUB.unmap(ctx.device);

        MultiVP* vp = (MultiVP*)vpUB.map(ctx.device);

        if (rCtx.enableVR) {
            vp->views[0] = rCtx.vrViewMats[0];
            vp->views[1] = rCtx.vrViewMats[1];
            vp->projections[0] = rCtx.vrProjMats[0];
            vp->projections[1] = rCtx.vrProjMats[1];
        } else {
            vp->views[0] = rCtx.cam.getViewMatrix();
            vp->projections[0] = rCtx.cam.getProjectionMatrix((float)rCtx.width / (float)rCtx.height);
            vp->viewPos[0] = glm::vec4(rCtx.cam.position, 0.0f);
        }

        vpUB.unmap(ctx.device);

        LightUB* lub = (LightUB*)lightsUB.map(ctx.device);
        glm::vec3 viewPos = rCtx.viewPos;

        int lightIdx = 0;
        rCtx.reg.view<WorldLight, Transform>().each([&lub, &lightIdx, &viewPos](auto ent, WorldLight& l, Transform& transform) {
            glm::vec3 lightForward = glm::normalize(transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f));
            if (l.type == LightType::Directional) {
                const float SHADOW_DISTANCE = 25.0f;
                glm::vec3 shadowMapPos = glm::round(viewPos - (transform.rotation * glm::vec3(0.0f, 0.f, 50.0f)));
                glm::mat4 proj = glm::orthoZO(
                    -SHADOW_DISTANCE, SHADOW_DISTANCE,
                    -SHADOW_DISTANCE, SHADOW_DISTANCE,
                    1.0f, 1000.f);

                glm::mat4 view = glm::lookAt(
                    shadowMapPos,
                    shadowMapPos - lightForward,
                    glm::vec3(0.0f, 1.0f, 0.0));

                lub->shadowmapMatrix = proj * view;
            }

            lub->lights[lightIdx] = PackedLight{
                glm::vec4(l.color, (float)l.type),
                glm::vec4(lightForward, l.spotCutoff),
                glm::vec4(transform.position, 0.0f) };
            lightIdx++;
            });

        lub->pack0.x = (float)lightIdx;
        lightsUB.unmap(ctx.device);

        if (rCtx.reuploadMats) {
            auto memoryProps = ctx.physicalDevice.getMemoryProperties();
            materialUB.upload(ctx.device, memoryProps, ctx.commandPool, ctx.device.getQueue(ctx.graphicsQueueFamilyIdx, 0), (*rCtx.materialSlots)->getSlots(), sizeof(PackedMaterial) * 256);

            // Update descriptor sets to bring in any new textures
            updateDescriptorSets(ctx);
        }

        {
            auto& renderBuffer = g_scene->getRenderBuffer();

            if (currentLineVBSize < renderBuffer.getNbLines() * 2) {
                currentLineVBSize = (renderBuffer.getNbLines() * 2) + 128;
                lineVB = vku::GenericBuffer{ ctx.device, ctx.allocator, vk::BufferUsageFlagBits::eVertexBuffer, sizeof(LineVert) * currentLineVBSize, VMA_MEMORY_USAGE_CPU_TO_GPU, "Line Buffer" };
            }

            if (currentLineVBSize) {
                LineVert* lineVBDat = (LineVert*)lineVB.map(ctx.device);
                for (int i = 0; i < renderBuffer.getNbLines(); i++) {
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
    }

    struct SubmeshDrawInfo {
        uint32_t materialIdx;
        uint32_t matrixIdx;
        vk::Buffer vb;
        vk::Buffer ib;
        uint32_t indexCount;
        uint32_t indexOffset;
        glm::vec4 texScaleOffset;
        entt::entity ent;
        vk::Pipeline pipeline;
    };

    void PolyRenderPass::execute(RenderCtx& ctx) {
#ifdef TRACY_ENABLE
        ZoneScoped;
        TracyVkZone((*ctx.tracyContexts)[ctx.imageIndex], *ctx.cmdBuf, "Polys");
#endif
        // Fast path clear values for AMD
        std::array<float, 4> clearColorValue{ 0.0f, 0.0f, 0.0f, 1 };
        vk::ClearDepthStencilValue clearDepthValue{ 1.0f, 0 };
        std::array<vk::ClearValue, 2> clearColours{ vk::ClearValue{clearColorValue}, clearDepthValue };
        vk::RenderPassBeginInfo rpbi;

        rpbi.renderPass = *renderPass;
        rpbi.framebuffer = *renderFb;
        rpbi.renderArea = vk::Rect2D{ {0, 0}, {ctx.width, ctx.height} };
        rpbi.clearValueCount = (uint32_t)clearColours.size();
        rpbi.pClearValues = clearColours.data();

        vk::UniqueCommandBuffer& cmdBuf = ctx.cmdBuf;
        entt::registry& reg = ctx.reg;
        Camera& cam = ctx.cam;

        vpUB.barrier(
            *cmdBuf, vk::PipelineStageFlagBits::eHost, vk::PipelineStageFlagBits::eVertexShader,
            vk::DependencyFlagBits::eByRegion, vk::AccessFlagBits::eHostWrite, vk::AccessFlagBits::eUniformRead,
            VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED);

        lightsUB.barrier(
            *cmdBuf, vk::PipelineStageFlagBits::eHost, vk::PipelineStageFlagBits::eFragmentShader,
            vk::DependencyFlagBits::eByRegion, vk::AccessFlagBits::eHostWrite, vk::AccessFlagBits::eUniformRead,
            VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED);

        if (pickThisFrame) {
            pickingBuffer.barrier(*cmdBuf, vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eComputeShader,
                vk::DependencyFlagBits::eByRegion,
                vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite,
                VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED);
            cmdBuf->bindPipeline(vk::PipelineBindPoint::eCompute, *pickingBufCsPipeline);
            cmdBuf->bindDescriptorSets(vk::PipelineBindPoint::eCompute, *pickingBufCsLayout, 0, *pickingBufCsDs, nullptr);
            PickBufCSPushConstants pbcspc;
            pbcspc.clearObjId = 1;
            pbcspc.doPicking = 1;
            cmdBuf->pushConstants<PickBufCSPushConstants>(*pickingBufCsLayout, vk::ShaderStageFlagBits::eCompute, 0, pbcspc);
            cmdBuf->dispatch(1, 1, 1);
            pickingBuffer.barrier(
                *cmdBuf, vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eFragmentShader,
                vk::DependencyFlagBits::eByRegion,
                vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
                VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED);
        }

        if (setEventNextFrame) {
            cmdBuf->setEvent(*pickEvent, vk::PipelineStageFlagBits::eAllCommands);
            setEventNextFrame = false;
        }

        cmdBuf->beginRenderPass(rpbi, vk::SubpassContents::eInline);


        int matrixIdx = 0;

        std::vector<SubmeshDrawInfo> drawInfo;
        drawInfo.reserve(reg.view<Transform, WorldObject>().size());

        matrixIdx = 0;
        cmdBuf->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, *descriptorSet, nullptr);
        //cmdBuf->bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);

        Frustum frustum;
        Frustum frustumB;

        if (!ctx.enableVR)
            frustum.fromVPMatrix(ctx.cam.getProjectionMatrix((float)ctx.width / (float)ctx.height) * ctx.cam.getViewMatrix());
        else {
            frustum.fromVPMatrix(ctx.vrProjMats[0] * ctx.vrViewMats[0]);
            frustumB.fromVPMatrix(ctx.vrProjMats[1] * ctx.vrViewMats[1]);
        }

        reg.view<Transform, WorldObject>().each([&, this](entt::entity ent, Transform& transform, WorldObject& obj) {
            ZoneScoped;
            auto meshPos = ctx.loadedMeshes.find(obj.mesh);

            if (meshPos == ctx.loadedMeshes.end()) {
                // Haven't loaded the mesh yet
                matrixIdx++;
                logWarn(WELogCategoryRender, "Missing mesh");
                return;
            }

            float maxScale = glm::max(transform.scale.x, glm::max(transform.scale.y, transform.scale.z));
            if (!ctx.enableVR) {
                //if (!frustum.containsSphere(transform.position, meshPos->second.sphereRadius * maxScale)) {
                glm::vec3 scaledMin = transform.scale * meshPos->second.aabbMin;
                glm::vec3 scaledMax = transform.scale * meshPos->second.aabbMax;

                if (!frustum.containsSphere(transform.position, meshPos->second.sphereRadius * maxScale)) {
                    ctx.dbgStats->numCulledObjs++;
                    matrixIdx++;
                    return;
                }
            } else {
                if (!frustum.containsSphere(transform.position, meshPos->second.sphereRadius * maxScale) && !frustumB.containsSphere(transform.position, meshPos->second.sphereRadius * maxScale)) {
                    ctx.dbgStats->numCulledObjs++;
                    matrixIdx++;
                    return;
                }
            }
            for (int i = 0; i < meshPos->second.numSubmeshes; i++) {
                auto& currSubmesh = meshPos->second.submeshes[i];

                SubmeshDrawInfo sdi;
                sdi.ib = meshPos->second.ib.buffer();
                sdi.vb = meshPos->second.vb.buffer();
                sdi.indexCount = currSubmesh.indexCount;
                sdi.indexOffset = currSubmesh.indexOffset;
                sdi.materialIdx = obj.materialIdx[i];
                sdi.matrixIdx = matrixIdx;
                sdi.texScaleOffset = obj.texScaleOffset;
                sdi.ent = ent;

                auto& extraDat = ctx.materialSlots->get()->getExtraDat(obj.materialIdx[i]);
                if (extraDat.noCull) {
                    sdi.pipeline = *noBackfaceCullPipeline;
                } else if (extraDat.wireframe) {
                    sdi.pipeline = *wireframePipeline;
                } else if (reg.has<UseWireframe>(ent)) {
                    sdi.pipeline = *pipeline;
                    drawInfo.push_back(sdi);
                    sdi.pipeline = *wireframePipeline;
                } else {
                    sdi.pipeline = *pipeline;
                }

                drawInfo.emplace_back(std::move(sdi));
                ctx.dbgStats->numDrawCalls++;
            }
            matrixIdx++;
            });

        std::sort(drawInfo.begin(), drawInfo.end(), [](SubmeshDrawInfo& a, SubmeshDrawInfo& b) {
            return a.pipeline < b.pipeline;
            });

        cmdBuf->bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);

        SubmeshDrawInfo last;
        last.pipeline = *pipeline;
        for (auto& sdi : drawInfo) {
            if (last.pipeline != sdi.pipeline) {
                cmdBuf->bindPipeline(vk::PipelineBindPoint::eGraphics, sdi.pipeline);
            }

            StandardPushConstants pushConst{ sdi.texScaleOffset, glm::ivec4(sdi.matrixIdx, sdi.materialIdx, 0, sdi.ent), glm::ivec4(pickX, pickY, 0, 0) };
            cmdBuf->pushConstants<StandardPushConstants>(*pipelineLayout, vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex, 0, pushConst);
            cmdBuf->bindVertexBuffers(0, sdi.vb, vk::DeviceSize(0));
            cmdBuf->bindIndexBuffer(sdi.ib, 0, vk::IndexType::eUint32);
            cmdBuf->drawIndexed(sdi.indexCount, 1, sdi.indexOffset, 0, 0);

            last = sdi;
        }

        cmdBuf->bindPipeline(vk::PipelineBindPoint::eGraphics, *noBackfaceCullPipeline);

        cmdBuf->bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
        /*for (auto& sdi : noCullObjs) {
            StandardPushConstants pushConst{ sdi.texScaleOffset, glm::ivec4(sdi.matrixIdx, sdi.materialIdx, 0, sdi.ent), glm::ivec4(pickX, pickY, 0, 0) };
            cmdBuf->pushConstants<StandardPushConstants>(*pipelineLayout, vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex, 0, pushConst);
            cmdBuf->bindVertexBuffers(0, sdi.vb, vk::DeviceSize(0));
            cmdBuf->bindIndexBuffer(sdi.ib, 0, vk::IndexType::eUint32);
            cmdBuf->drawIndexed(sdi.indexCount, 1, sdi.indexOffset, 0, 0);
        }*/

        reg.view<Transform, ProceduralObject>().each([this, &cmdBuf, &cam, &matrixIdx](auto ent, Transform& transform, ProceduralObject& obj) {
            if (!obj.visible) return;
            StandardPushConstants pushConst{ glm::vec4(1.0f, 1.0f, 0.0f, 0.0f), glm::ivec4(matrixIdx, obj.materialIdx, 0, ent), glm::ivec4(pickX, pickY, 0, 0) };
            cmdBuf->pushConstants<StandardPushConstants>(*pipelineLayout, vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex, 0, pushConst);
            cmdBuf->bindVertexBuffers(0, obj.vb.buffer(), vk::DeviceSize(0));
            cmdBuf->bindIndexBuffer(obj.ib.buffer(), 0, obj.indexType);
            cmdBuf->drawIndexed(obj.indexCount, 1, 0, 0, 0);
            matrixIdx++;
            });

        if (matrixIdx >= 1024) {
            fatalErr("Out of model matrices!");
        }

        if (numLineVerts > 0) {
            cmdBuf->bindPipeline(vk::PipelineBindPoint::eGraphics, *linePipeline);
            cmdBuf->bindVertexBuffers(0, lineVB.buffer(), vk::DeviceSize(0));
            cmdBuf->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *linePipelineLayout, 0, lineDs, nullptr);
            cmdBuf->draw(numLineVerts, 1, 0, 0);
        }

        /*cmdBuf->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *wireframePipelineLayout, 0, *wireframeDescriptorSet, nullptr);
        cmdBuf->bindPipeline(vk::PipelineBindPoint::eGraphics, *wireframePipeline);

        for (auto& sdi : wireframeObjs) {
            StandardPushConstants pushConst{ sdi.texScaleOffset, glm::ivec4(sdi.matrixIdx, sdi.materialIdx, 0, sdi.ent), glm::ivec4(pickX, pickY, 0, 0) };
            cmdBuf->pushConstants<StandardPushConstants>(*pipelineLayout, vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex, 0, pushConst);
            cmdBuf->bindVertexBuffers(0, sdi.vb, vk::DeviceSize(0));
            cmdBuf->bindIndexBuffer(sdi.ib, 0, vk::IndexType::eUint32);
            cmdBuf->drawIndexed(sdi.indexCount, 1, sdi.indexOffset, 0, 0);
        }*/

        cmdBuf->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *skyboxPipelineLayout, 0, *skyboxDs, nullptr);
        cmdBuf->bindPipeline(vk::PipelineBindPoint::eGraphics, *skyboxPipeline);

        SkyboxPushConstants spc{ glm::ivec4(0) };
        cmdBuf->pushConstants<SkyboxPushConstants>(*skyboxPipelineLayout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, spc);

        cmdBuf->draw(36, 1, 0, 0);

        cmdBuf->endRenderPass();

        if (pickThisFrame) {
            cmdBuf->bindPipeline(vk::PipelineBindPoint::eCompute, *pickingBufCsPipeline);
            cmdBuf->bindDescriptorSets(vk::PipelineBindPoint::eCompute, *pickingBufCsLayout, 0, *pickingBufCsDs, nullptr);
            pickingBuffer.barrier(
                *cmdBuf, vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eComputeShader,
                vk::DependencyFlagBits::eByRegion,
                vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderWrite,
                VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED);
            PickBufCSPushConstants pbcspc;
            pbcspc.clearObjId = 0;
            pbcspc.doPicking = 0;
            cmdBuf->pushConstants<PickBufCSPushConstants>(*pickingBufCsLayout, vk::ShaderStageFlagBits::eCompute, 0, pbcspc);
            cmdBuf->dispatch(1, 1, 1);

            cmdBuf->resetEvent(*pickEvent, vk::PipelineStageFlagBits::eComputeShader);
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
        MultiVP* multivp = (MultiVP*)vpUB.map(dev);
        multivp->views[0] = views[0];
        multivp->views[1] = views[1];
        multivp->viewPos[0] = glm::vec4(viewPos[0], 0.0f);
        multivp->viewPos[1] = glm::vec4(viewPos[1], 0.0f);
        vpUB.unmap(dev);
    }

    PolyRenderPass::~PolyRenderPass() {
    }
}