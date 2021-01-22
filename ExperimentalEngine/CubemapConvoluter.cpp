#include "Engine.hpp"
#include "TimingUtil.hpp"
#include "Render.hpp"

namespace worlds {
    struct PrefilterPushConstants {
        float roughness;
        int faceIdx;
    };

    CubemapConvoluter::CubemapConvoluter(std::shared_ptr<VulkanHandles> ctx) : vkCtx(ctx) {
        cs = vku::loadShaderAsset(ctx->device, g_assetDB.addOrGetExisting("Shaders/cubemap_prefilter.comp.spv"));

        vku::DescriptorSetLayoutMaker dslm;
        dslm.image(0, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute, 1);
        dslm.image(1, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute, 1);
        dslm.image(2, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute, 1);
        dsl = dslm.createUnique(ctx->device);

        vku::PipelineLayoutMaker plm;
        plm.pushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(PrefilterPushConstants));
        plm.descriptorSetLayout(*dsl);
        pipelineLayout = plm.createUnique(ctx->device);

        vku::ComputePipelineMaker cpm;
        cpm.shader(vk::ShaderStageFlagBits::eCompute, cs);
        pipeline = cpm.createUnique(ctx->device, ctx->pipelineCache, *pipelineLayout);

        vku::SamplerMaker sm;
        sampler = sm.createUnique(ctx->device);
    }

    void CubemapConvoluter::convolute(vku::TextureImageCube& cube) {
        PerfTimer pt; 
        if (cube.info().mipLevels == 1) {
            throw std::runtime_error("Can't convolute cubemap with 1 miplevel");
        }

        uint32_t cubemapWidth = cube.extent().width;
        uint32_t cubemapHeight = cube.extent().height;

        std::vector<vk::DescriptorPoolSize> poolSizes;
        poolSizes.emplace_back(vk::DescriptorType::eCombinedImageSampler, 1024);
        poolSizes.emplace_back(vk::DescriptorType::eStorageBuffer, 1024);

        vk::DescriptorPoolCreateInfo descriptorPoolInfo{};
        descriptorPoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
        descriptorPoolInfo.maxSets = cube.info().mipLevels * 6;
        descriptorPoolInfo.poolSizeCount = (uint32_t)poolSizes.size();
        descriptorPoolInfo.pPoolSizes = poolSizes.data();

        vk::DescriptorPool tmpDescriptorPool = vkCtx->device.createDescriptorPool(descriptorPoolInfo);

        vku::DescriptorSetMaker dsm;
        for (uint32_t i = 1; i < cube.info().mipLevels; i++) {
            for (int j = 0; j < 6; j++) {
                dsm.layout(*dsl);
            }
        }

        auto descriptorSets = dsm.create(vkCtx->device, tmpDescriptorPool);
        std::vector<vk::UniqueImageView> outputViews;
        std::vector<vk::UniqueImageView> inputViews;

        for (uint32_t i = 1; i < cube.info().mipLevels; i++) {
            for (int j = 0; j < 6; j++) {
                vk::ImageViewCreateInfo viewInfo{};
                viewInfo.image = cube.image();
                viewInfo.viewType = vk::ImageViewType::e2D;
                viewInfo.format = cube.format();
                viewInfo.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA };
                viewInfo.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, (uint32_t)i, 1, (uint32_t)j, 1 };
                outputViews.push_back(vkCtx->device.createImageViewUnique(viewInfo));
            }
        }

        for (int i = 0; i < 6; i++) {
            vk::ImageViewCreateInfo viewInfo{};
            viewInfo.image = cube.image();
            viewInfo.viewType = vk::ImageViewType::e2D;
            viewInfo.format = cube.format();
            viewInfo.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA };
            viewInfo.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, (uint32_t)i, 1 };
            inputViews.push_back(vkCtx->device.createImageViewUnique(viewInfo));
        }

        vku::DescriptorSetUpdater dsu(0, descriptorSets.size() * 3, 0);
        for (int i = 0; i < descriptorSets.size(); i++) {
            int arrayIdx = (i % 6);

            dsu.beginDescriptorSet(descriptorSets[i]);
            dsu.beginImages(0, 0, vk::DescriptorType::eStorageImage);
            dsu.image(*sampler, *inputViews[arrayIdx], vk::ImageLayout::eGeneral);

            dsu.beginImages(1, 0, vk::DescriptorType::eStorageImage);
            dsu.image(*sampler, *outputViews[i], vk::ImageLayout::eGeneral);

            dsu.beginImages(2, 0, vk::DescriptorType::eCombinedImageSampler);
            dsu.image(*sampler, cube.imageView(), vk::ImageLayout::eGeneral);
        }

        assert(dsu.ok());

        dsu.update(vkCtx->device);

        vk::QueryPoolCreateInfo qpci;
        qpci.queryCount = 2;
        qpci.queryType = vk::QueryType::eTimestamp;
        auto qp = vkCtx->device.createQueryPoolUnique(qpci);

        auto period = vkCtx->physicalDevice.getProperties().limits.timestampPeriod;

        for (int i = 0; i < descriptorSets.size(); i++) {
            vku::executeImmediately(vkCtx->device, vkCtx->commandPool, vkCtx->device.getQueue(vkCtx->graphicsQueueFamilyIdx, 0),
                [&](vk::CommandBuffer cb) {
                    cb.resetQueryPool(*qp, 0, 2);
                    cb.writeTimestamp(vk::PipelineStageFlagBits::eComputeShader, *qp, 0);
                    cube.setLayout(cb, vk::ImageLayout::eGeneral);
                    cb.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline);

                    int mipLevel = (i / 6) + 1;
                    int arrayIdx = (i % 6);

                    auto width = vku::mipScale(cubemapWidth, mipLevel);
                    auto height = vku::mipScale(cubemapHeight, mipLevel);

                    PrefilterPushConstants ppc;
                    ppc.faceIdx = arrayIdx;
                    ppc.roughness = (float)mipLevel / (float)cube.info().mipLevels;

                    cb.pushConstants<PrefilterPushConstants>(*pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, ppc);
                    cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *pipelineLayout, 0, descriptorSets[i], nullptr);
                    cb.dispatch((width + 15) / 16, (height + 15) / 16, 1);
                    cube.setLayout(cb, vk::ImageLayout::eShaderReadOnlyOptimal);
                    cb.writeTimestamp(vk::PipelineStageFlagBits::eComputeShader, *qp, 1);
                });
            vkCtx->device.waitIdle();
            auto qr = vkCtx->device.getQueryPoolResults<uint64_t>(*qp, 
                0, 2, 
                sizeof(uint64_t) * 2, sizeof(uint64_t), 
                vk::QueryResultFlagBits::eWait);

            uint64_t duration = qr.value[1] - qr.value[0];
            logMsg("convolution of %i took %.3f ms", i, (duration * period) / 1000.0f / 1000.0f);

            // Delay to try and avoid random lockups that occur on AMD
            if ((duration * period) / 1000.0f / 1000.0f > 50.0f) {
                SDL_Delay(50);
            }
        }

        vkCtx->device.destroyDescriptorPool(tmpDescriptorPool);
        logMsg("cubemap convolution took %fms", pt.stopGetMs());
    }
}
