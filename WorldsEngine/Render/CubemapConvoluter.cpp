#include "../Core/Engine.hpp"
#include "../Util/TimingUtil.hpp"
#include "../Render/Render.hpp"

namespace worlds {
    struct PrefilterPushConstants {
        float roughness;
        int faceIdx;
    };

    CubemapConvoluter::CubemapConvoluter(std::shared_ptr<VulkanHandles> ctx) : vkCtx(ctx) {
        cs = vku::loadShaderAsset(ctx->device, g_assetDB.addOrGetExisting("Shaders/cubemap_prefilter.comp.spv"));

        vku::DescriptorSetLayoutMaker dslm;
        dslm.image(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute, 1);
        dslm.image(1, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute, 1);
        dsl = dslm.createUnique(ctx->device);

        vku::PipelineLayoutMaker plm;
        plm.pushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(PrefilterPushConstants));
        plm.descriptorSetLayout(*dsl);
        pipelineLayout = plm.createUnique(ctx->device);

        vku::ComputePipelineMaker cpm;
        cpm.shader(vk::ShaderStageFlagBits::eCompute, cs);
        pipeline = cpm.createUnique(ctx->device, ctx->pipelineCache, *pipelineLayout);

        vku::SamplerMaker sm;
        sm.magFilter(vk::Filter::eLinear).minFilter(vk::Filter::eLinear).mipmapMode(vk::SamplerMipmapMode::eLinear).maxLod(VK_LOD_CLAMP_NONE).minLod(0.0f);
        sampler = sm.createUnique(ctx->device);
    }

    void generateMipCube(const VulkanHandles& vkCtx, vku::TextureImageCube& src, vku::TextureImageCube& t, vk::CommandBuffer cb) {
        auto currLayout = src.layout();
        vk::ImageMemoryBarrier imb;
        imb.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, src.info().mipLevels, 0, 6 };

        imb.image = src.image();
        imb.oldLayout = currLayout;
        imb.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        imb.srcAccessMask = vk::AccessFlagBits::eHostWrite;
        imb.dstAccessMask = vk::AccessFlagBits::eTransferRead;

        vk::ImageMemoryBarrier imb2;
        imb2.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, t.info().mipLevels, 0, 6 };

        imb2.image = t.image();
        imb2.oldLayout = t.layout();
        imb2.newLayout = vk::ImageLayout::eTransferDstOptimal;
        imb2.srcAccessMask = vk::AccessFlagBits::eHostWrite;
        imb2.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
        cb.pipelineBarrier(vk::PipelineStageFlagBits::eHost, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlagBits::eByRegion, nullptr, nullptr, { imb, imb2 });

        int32_t mipWidth = t.info().extent.width;
        int32_t mipHeight = t.info().extent.height;
        for (uint32_t i = 0; i < t.info().mipLevels; i++) {
            vk::ImageBlit ib;
            ib.dstSubresource = vk::ImageSubresourceLayers{ vk::ImageAspectFlagBits::eColor, i, 0, 6 };
            ib.dstOffsets[0] = vk::Offset3D{ 0, 0, 0 };
            ib.dstOffsets[1] = vk::Offset3D{ mipWidth, mipHeight, 1 };
            ib.srcSubresource = vk::ImageSubresourceLayers{ vk::ImageAspectFlagBits::eColor, 0, 0, 6 };
            ib.srcOffsets[0] = vk::Offset3D{ 0, 0, 0 };
            ib.srcOffsets[1] = vk::Offset3D{ (int32_t)t.info().extent.width, (int32_t)t.info().extent.height, 1 };

            cb.blitImage(src.image(), vk::ImageLayout::eTransferSrcOptimal, t.image(), vk::ImageLayout::eTransferDstOptimal, ib, vk::Filter::eLinear);

            mipWidth /= 2;
            mipHeight /= 2;
        }

        vk::ImageMemoryBarrier imb4;
        imb4.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, src.info().mipLevels, 0, 6 };
        imb4.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
        imb4.newLayout = currLayout;
        imb4.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        imb4.dstAccessMask = vk::AccessFlagBits::eShaderWrite;
        imb4.image = src.image();

        vk::ImageMemoryBarrier imb3;
        imb3.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, t.info().mipLevels, 0, 6 };
        imb3.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        imb3.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        imb3.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        imb3.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        imb3.image = t.image();

        cb.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
            vk::DependencyFlagBits::eByRegion, nullptr, nullptr, { imb3, imb4 });
    }

    void CubemapConvoluter::convolute(vku::TextureImageCube& cube) {
        PerfTimer pt;
        if (cube.info().mipLevels == 1) {
            throw std::runtime_error("Can't convolute cubemap with 1 miplevel");
        }

        uint32_t cubemapWidth = cube.extent().width;
        uint32_t cubemapHeight = cube.extent().height;

        vku::TextureImageCube mipCube {
            vkCtx->device, vkCtx->allocator,
            cubemapWidth, cubemapHeight,
            cube.info().mipLevels,
            cube.info().format
        };

        vku::executeImmediately(vkCtx->device, vkCtx->commandPool, vkCtx->device.getQueue(vkCtx->graphicsQueueFamilyIdx, 0),
            [&](vk::CommandBuffer cb) {
                generateMipCube(*vkCtx, cube, mipCube, cb);
            }
        );

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

        vku::DescriptorSetUpdater dsu(0, descriptorSets.size() * 3, 0);
        for (int i = 0; i < (int)descriptorSets.size(); i++) {
            int arrayIdx = (i % 6);

            dsu.beginDescriptorSet(descriptorSets[i]);
            dsu.beginImages(0, 0, vk::DescriptorType::eCombinedImageSampler);
            dsu.image(*sampler, mipCube.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

            dsu.beginImages(1, 0, vk::DescriptorType::eStorageImage);
            dsu.image(*sampler, *outputViews[i], vk::ImageLayout::eGeneral);
        }

        assert(dsu.ok());

        dsu.update(vkCtx->device);

        vku::executeImmediately(vkCtx->device, vkCtx->commandPool, vkCtx->device.getQueue(vkCtx->graphicsQueueFamilyIdx, 0),
            [&](vk::CommandBuffer cb) {
                cube.setLayout(cb, vk::ImageLayout::eGeneral);
                for (int i = 0; i < (int)descriptorSets.size(); i++) {
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
                    cube.barrier(
                        cb,
                        vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader,
                        vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderWrite
                    );
                }
                cube.setLayout(cb, vk::ImageLayout::eShaderReadOnlyOptimal);
            });

        vkCtx->device.destroyDescriptorPool(tmpDescriptorPool);
        logMsg("cubemap convolution took %fms", pt.stopGetMs());
    }
}
