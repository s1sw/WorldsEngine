#include "../Core/Engine.hpp"
#include "../Util/TimingUtil.hpp"
#include "../Render/RenderInternal.hpp"

namespace worlds {
    struct PrefilterPushConstants {
        float roughness;
        int faceIdx;
    };

    CubemapConvoluter::CubemapConvoluter(std::shared_ptr<VulkanHandles> ctx) : vkCtx(ctx) {
        cs = vku::loadShaderAsset(ctx->device, AssetDB::pathToId("Shaders/cubemap_prefilter.comp.spv"));

        vku::DescriptorSetLayoutMaker dslm;
        dslm.image(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 1);
        dslm.image(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 1);
        dsl = dslm.create(ctx->device);

        vku::PipelineLayoutMaker plm;
        plm.pushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PrefilterPushConstants));
        plm.descriptorSetLayout(dsl);
        pipelineLayout = plm.create(ctx->device);

        vku::ComputePipelineMaker cpm;
        cpm.shader(VK_SHADER_STAGE_COMPUTE_BIT, cs);
        pipeline = cpm.create(ctx->device, ctx->pipelineCache, pipelineLayout);

        vku::SamplerMaker sm;
        sm.magFilter(VK_FILTER_LINEAR).minFilter(VK_FILTER_LINEAR).mipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR).maxLod(VK_LOD_CLAMP_NONE).minLod(0.0f);
        sampler = sm.create(ctx->device);
    }

    void generateMipCube(const VulkanHandles& vkCtx, vku::TextureImageCube& src, vku::TextureImageCube& t, VkCommandBuffer cb) {
        auto currLayout = src.layout();
        VkImageMemoryBarrier imb{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        imb.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, src.info().mipLevels, 0, 6 };

        imb.image = src.image();
        imb.oldLayout = currLayout;
        imb.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        imb.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        imb.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        VkImageMemoryBarrier imb2{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        imb2.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, t.info().mipLevels, 0, 6 };

        imb2.image = t.image();
        imb2.oldLayout = t.layout();
        imb2.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imb2.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        imb2.dstAccessMask = VK_PIPELINE_STAGE_TRANSFER_BIT;

        VkImageMemoryBarrier barriers[2] = { imb, imb2 };
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 
            0, nullptr, 
            0, nullptr, 
            2, barriers);

        int32_t mipWidth = t.info().extent.width;
        int32_t mipHeight = t.info().extent.height;
        for (uint32_t i = 0; i < t.info().mipLevels; i++) {
            VkImageBlit ib;
            ib.dstSubresource = VkImageSubresourceLayers{ VK_IMAGE_ASPECT_COLOR_BIT, i, 0, 6 };
            ib.dstOffsets[0] = VkOffset3D{ 0, 0, 0 };
            ib.dstOffsets[1] = VkOffset3D{ mipWidth, mipHeight, 1 };
            ib.srcSubresource = VkImageSubresourceLayers{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 6 };
            ib.srcOffsets[0] = VkOffset3D{ 0, 0, 0 };
            ib.srcOffsets[1] = VkOffset3D{ (int32_t)t.info().extent.width, (int32_t)t.info().extent.height, 1 };

            vkCmdBlitImage(cb, 
                src.image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                t.image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &ib, VK_FILTER_LINEAR);

            mipWidth /= 2;
            mipHeight /= 2;
        }

        VkImageMemoryBarrier imb4{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        imb4.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, src.info().mipLevels, 0, 6 };
        imb4.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        imb4.newLayout = currLayout;
        imb4.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        imb4.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        imb4.image = src.image();

        VkImageMemoryBarrier imb3{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        imb3.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, t.info().mipLevels, 0, 6 };
        imb3.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imb3.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imb3.srcAccessMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        imb3.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        imb3.image = t.image();

        VkImageMemoryBarrier barriers2[2] = { imb3, imb4 };
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_DEPENDENCY_BY_REGION_BIT, 
            0, nullptr, 
            0, nullptr, 
            2, barriers2);
    }

    void CubemapConvoluter::convolute(vku::TextureImageCube& cube) {
        PerfTimer pt;

        if (cube.info().mipLevels == 1) {
            throw std::runtime_error("Can't convolute cubemap with 1 miplevel");
        }

        uint32_t cubemapWidth = cube.extent().width;
        uint32_t cubemapHeight = cube.extent().height;

        // Create another cubemap with full mips for prefiltering
        vku::TextureImageCube mipCube {
            vkCtx->device, vkCtx->allocator,
            cubemapWidth, cubemapHeight,
            cube.info().mipLevels,
            cube.info().format
        };

        VkQueue queue;
        vkGetDeviceQueue(vkCtx->device, vkCtx->graphicsQueueFamilyIdx, 0, &queue);
        vku::executeImmediately(vkCtx->device, vkCtx->commandPool, queue,
            [&](VkCommandBuffer cb) {
                generateMipCube(*vkCtx, cube, mipCube, cb);
            }
        );

        std::vector<VkDescriptorPoolSize> poolSizes;
        poolSizes.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024);
        poolSizes.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1024);

        VkDescriptorPoolCreateInfo descriptorPoolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        descriptorPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        descriptorPoolInfo.maxSets = cube.info().mipLevels * 6;
        descriptorPoolInfo.poolSizeCount = (uint32_t)poolSizes.size();
        descriptorPoolInfo.pPoolSizes = poolSizes.data();

        VkDescriptorPool tmpDescriptorPool;
        vkCreateDescriptorPool(vkCtx->device, &descriptorPoolInfo, nullptr, &tmpDescriptorPool);

        vku::DescriptorSetMaker dsm;
        for (uint32_t i = 1; i < cube.info().mipLevels; i++) {
            for (int j = 0; j < 6; j++) {
                dsm.layout(dsl);
            }
        }

        auto descriptorSets = dsm.create(vkCtx->device, tmpDescriptorPool);
        std::vector<VkImageView> outputViews;

        for (uint32_t i = 1; i < cube.info().mipLevels; i++) {
            for (int j = 0; j < 6; j++) {
                VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
                viewInfo.image = cube.image();
                viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                viewInfo.format = cube.format();
                viewInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
                viewInfo.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, (uint32_t)i, 1, (uint32_t)j, 1 };

                VkImageView imageView;
                VKCHECK(vkCreateImageView(vkCtx->device, &viewInfo, nullptr, &imageView));
                outputViews.push_back(imageView);
            }
        }

        vku::DescriptorSetUpdater dsu(0, descriptorSets.size() * 3, 0);
        for (int i = 0; i < (int)descriptorSets.size(); i++) {
            int arrayIdx = (i % 6);

            dsu.beginDescriptorSet(descriptorSets[i]);
            dsu.beginImages(0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            dsu.image(sampler, mipCube.imageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            dsu.beginImages(1, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
            dsu.image(sampler, outputViews[i], VK_IMAGE_LAYOUT_GENERAL);
        }

        assert(dsu.ok());

        dsu.update(vkCtx->device);

        vku::executeImmediately(vkCtx->device, vkCtx->commandPool, queue,
            [&](VkCommandBuffer cb) {
                cube.setLayout(cb, VK_IMAGE_LAYOUT_GENERAL);
                for (int i = 0; i < (int)descriptorSets.size(); i++) {
                    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

                    int mipLevel = (i / 6) + 1;
                    int arrayIdx = (i % 6);

                    auto width = vku::mipScale(cubemapWidth, mipLevel);
                    auto height = vku::mipScale(cubemapHeight, mipLevel);

                    PrefilterPushConstants ppc;
                    ppc.faceIdx = arrayIdx;
                    ppc.roughness = (float)mipLevel / (float)cube.info().mipLevels;

                    vkCmdPushConstants(cb, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ppc), &ppc);
                    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSets[i], 0, nullptr);
                    vkCmdDispatch(cb, (width + 15) / 16, (height + 15) / 16, 1);
                    cube.barrier(
                        cb,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT
                    );
                }
                cube.setLayout(cb, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            });

        for (VkImageView iv : outputViews) {
            vkDestroyImageView(vkCtx->device, iv, nullptr);
        }
        vkDestroyDescriptorPool(vkCtx->device, tmpDescriptorPool, nullptr);
        logMsg("cubemap convolution took %fms", pt.stopGetMs());
    }
}
