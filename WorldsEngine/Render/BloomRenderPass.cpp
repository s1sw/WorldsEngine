#include "RenderPasses.hpp"
#include "ShaderReflector.hpp"
#include "ShaderCache.hpp"
#include "vku/DescriptorSetUtil.hpp"

namespace worlds {
    BloomRenderPass::BloomRenderPass(VulkanHandles* handles, RenderResource* hdrImg, RenderResource* bloomTarget)
        : RenderPass(handles)
        , hdrImg(hdrImg)
        , bloomTarget(bloomTarget) {}

    void BloomRenderPass::setupApplyShader(RenderContext& ctx, VkDescriptorPool descriptorPool) {
        AssetID shaderId = AssetDB::pathToId("Shaders/bloom.comp.spv");

        applyDsl = ShaderReflector{ shaderId }.createDescriptorSetLayout(handles->device, 0);

        vku::PipelineLayoutMaker plm;
        plm.descriptorSetLayout(applyDsl);
        applyPipelineLayout = plm.create(handles->device);

        vku::ComputePipelineMaker cpm;
        cpm.shader(VK_SHADER_STAGE_COMPUTE_BIT, ShaderCache::getModule(handles->device, shaderId));

        applyPipeline = cpm.create(handles->device, handles->pipelineCache, applyPipelineLayout);

        vku::SamplerMaker sm;
        sm.magFilter(VK_FILTER_LINEAR).minFilter(VK_FILTER_LINEAR).minLod(0.0f).maxLod(16.0f).mipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR);
        sm.addressModeU(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE).addressModeV(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        sampler = sm.create(handles->device);

        vku::DescriptorSetMaker dsm;
        dsm.layout(applyDsl);
        applyDescriptorSet = dsm.create(handles->device, descriptorPool)[0];
    }

    struct BloomBlurPC {
        glm::vec2 direction;
        uint32_t inputMipLevel;
        float resScalar;
        glm::uvec2 resolution;
        bool useUpsampleFilter;
    };

    void BloomRenderPass::setupBlurShader(RenderContext& ctx, VkDescriptorPool descriptorPool) {
        AssetID shaderId = AssetDB::pathToId("Shaders/bloom_blur.comp.spv");

        blurDsl = ShaderReflector{ shaderId }.createDescriptorSetLayout(handles->device, 0);

        vku::PipelineLayoutMaker plm;
        plm.descriptorSetLayout(blurDsl);
        plm.pushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(BloomBlurPC));

        blurPipelineLayout = plm.create(handles->device);

        vku::ComputePipelineMaker cpm;
        cpm.shader(VK_SHADER_STAGE_COMPUTE_BIT, ShaderCache::getModule(handles->device, shaderId));
        blurPipeline = cpm.create(handles->device, handles->pipelineCache, blurPipelineLayout);

        AssetID seedShaderId = AssetDB::pathToId("Shaders/bloom_blur_seed.comp.spv");

        vku::ComputePipelineMaker cpm2;
        cpm2.shader(VK_SHADER_STAGE_COMPUTE_BIT, ShaderCache::getModule(handles->device, seedShaderId));
        seedBlurPipeline = cpm2.create(handles->device, handles->pipelineCache, blurPipelineLayout);

        for (int i = 0; i < nMips; i++) {
            VkImageViewCreateInfo ivci { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
            ivci.image = mipChain->image().image();
            ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
            ivci.components = VkComponentMapping { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
            ivci.format = VK_FORMAT_B10G11R11_UFLOAT_PACK32;
            ivci.subresourceRange = VkImageSubresourceRange {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = (uint32_t)i,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            };

            VkImageView view;
            VKCHECK(vkCreateImageView(handles->device, &ivci, nullptr, &view));
            blurMipChainImageViews.push_back(view);
        }

        vku::DescriptorSetMaker dsm;
        for (int i = 0; i < nMips; i++) {
            dsm.layout(blurDsl);
        }

        intermediateToChainDS = dsm.create(handles->device, descriptorPool);
        chainToChainDS = dsm.create(handles->device, descriptorPool);

        vku::DescriptorSetMaker dsm2;
        dsm2.layout(blurDsl);

        chainToIntermediateDS = dsm2.create(handles->device, descriptorPool)[0];

        vku::DescriptorSetMaker dsm3;
        dsm3.layout(blurDsl);
        hdrToChainDS = dsm3.create(handles->device, descriptorPool)[0];

        vku::DescriptorSetUpdater dsu(0, nMips * 4 + 4, 0);
        dsu.beginDescriptorSet(chainToIntermediateDS);
        dsu.beginImages(0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        dsu.image(sampler, mipChain->image().imageView(), VK_IMAGE_LAYOUT_GENERAL);
        dsu.beginImages(1, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        dsu.image(sampler, blurIntermediate->image().imageView(), VK_IMAGE_LAYOUT_GENERAL);

        for (int i = 0; i < nMips; i++) {
            dsu.beginDescriptorSet(intermediateToChainDS[i]);
            dsu.beginImages(0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            dsu.image(sampler, blurIntermediate->image().imageView(), VK_IMAGE_LAYOUT_GENERAL);
            dsu.beginImages(1, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
            dsu.image(sampler, blurMipChainImageViews[i], VK_IMAGE_LAYOUT_GENERAL);

            dsu.beginDescriptorSet(chainToChainDS[i]);
            dsu.beginImages(0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            dsu.image(sampler, mipChain->image().imageView(), VK_IMAGE_LAYOUT_GENERAL);
            dsu.beginImages(1, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
            dsu.image(sampler, blurMipChainImageViews[i], VK_IMAGE_LAYOUT_GENERAL);
        }

        dsu.beginDescriptorSet(hdrToChainDS);
        dsu.beginImages(0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        dsu.image(sampler, hdrImg->image().imageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        dsu.beginImages(1, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        dsu.image(sampler, blurMipChainImageViews[0], VK_IMAGE_LAYOUT_GENERAL);
        dsu.update(handles->device);
    }

    ConVar maxMips { "r_bloomMaxMips", "5" };
    void BloomRenderPass::setup(RenderContext& ctx, VkDescriptorPool descriptorPool) {
        VkExtent3D hdrExtent = hdrImg->image().extent();
        TextureResourceCreateInfo rci{
            TextureType::T2D,
            VK_FORMAT_B10G11R11_UFLOAT_PACK32,
            (int)hdrExtent.width, (int)hdrExtent.height,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
        };

        nMips = (int)floor(log2(std::max(hdrExtent.width, hdrExtent.height)));
        nMips = std::min(nMips, maxMips.getInt());
        rci.mipLevels = nMips;

        mipChain = ctx.renderer->createTextureResource(rci, "Bloom mip chain");

        TextureResourceCreateInfo intermediateCi{
            TextureType::T2D,
            VK_FORMAT_B10G11R11_UFLOAT_PACK32,
            (int)hdrExtent.width, (int)hdrExtent.height,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
        };

        blurIntermediate = ctx.renderer->createTextureResource(intermediateCi, "Intermediate texture");

        setupApplyShader(ctx, descriptorPool);
        setupBlurShader(ctx, descriptorPool);

        vku::DescriptorSetUpdater dsu;
        dsu.beginDescriptorSet(applyDescriptorSet);
        dsu.beginImages(0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        dsu.image(sampler, mipChain->image().imageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        dsu.beginImages(1, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        dsu.image(sampler, bloomTarget->image().imageView(), VK_IMAGE_LAYOUT_GENERAL);
        dsu.update(handles->device);
    }

    void transitionOneMip(VkImageMemoryBarrier& imb, vku::GenericImage& img, int mipLevel, VkImageLayout newLayout,
            VkAccessFlags srcAccess, VkAccessFlags dstAccess) {
        imb.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imb.subresourceRange = VkImageSubresourceRange{
           .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
           .baseMipLevel = (uint32_t)mipLevel,
           .levelCount = 1,
           .baseArrayLayer = 0,
           .layerCount = 1
        };

        imb.image = img.image();
        imb.oldLayout = img.layout();
        imb.newLayout = newLayout;
        imb.srcAccessMask = srcAccess;
        imb.dstAccessMask = dstAccess;
    }

    void BloomRenderPass::execute(RenderContext& ctx) {
        addDebugLabel(ctx.cmdBuf, "Bloom Pass", 1.0f, 0.5f, 0.5f, 1.0f);
        VkCommandBuffer cb = ctx.cmdBuf;

        mipChain->image().setLayout(cb, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);

        mipChain->image().setLayout(cb, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT);
        blurIntermediate->image().setLayout(cb, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT);

        // Perform initial copy to mip 0 of the chain
        VkExtent3D chainExtent = mipChain->image().extent();
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, seedBlurPipeline);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, blurPipelineLayout, 0, 1, &hdrToChainDS, 0, nullptr);
        BloomBlurPC pushConstants{
            .direction = glm::vec2(0.0f, 0.0f),
            .inputMipLevel = (uint32_t)(0),
            .resolution = glm::uvec2(chainExtent.width, chainExtent.height)
        };
        vkCmdPushConstants(cb, blurPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConstants), &pushConstants);
        vkCmdDispatch(cb, (chainExtent.width + 15) / 16, (chainExtent.height + 15) / 16, 1);

        VkClearColorValue clearVal{ 0.0f, 0.0f, 0.0f, 0.0f };
        VkImageSubresourceRange range{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        };
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, blurPipeline);
        for (int i = 1; i < nMips; i++) {
            VkExtent3D chainExtent = mipChain->image().extent();
            int thisWidth = vku::mipScale(chainExtent.width, i);
            int thisHeight = vku::mipScale(chainExtent.height, i);

            mipChain->image().barrier(cb,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
            vkCmdClearColorImage(cb, blurIntermediate->image().image(), VK_IMAGE_LAYOUT_GENERAL, &clearVal, 1, &range);
            blurIntermediate->image().barrier(cb,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT);
            // Blur from previous mip to intermediate
            vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, blurPipelineLayout, 0, 1, &chainToIntermediateDS, 0, nullptr);
            BloomBlurPC pushConstants{
                .direction = glm::vec2(1.0f, 0.0f),
                .inputMipLevel = (uint32_t)(i - 1),
                .resScalar = 1.0f,
                .resolution = glm::uvec2(thisWidth, thisHeight)
            };
            vkCmdPushConstants(cb, blurPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConstants), &pushConstants);
            vkCmdDispatch(cb, (thisWidth + 15) / 16, (thisHeight + 15) / 16, 1);

            // The intermediate buffer always has a size of chainExtent
            // Therefore when sampling from it, we must select the right region
            pushConstants.resScalar = (float)chainExtent.width / thisWidth;

            blurIntermediate->image().barrier(cb,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);

            // Vertical blur from intermediate to current mip
            vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, blurPipelineLayout, 0, 1, &intermediateToChainDS[i], 0, nullptr);
            pushConstants.direction = glm::vec2(0.0f, 1.0f);
            vkCmdPushConstants(cb, blurPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConstants), &pushConstants);
            vkCmdDispatch(cb, (thisWidth + 15) / 16, (thisHeight + 15) / 16, 1);
        }

        for (int i = nMips - 1; i > 0; i--) {
            VkExtent3D chainExtent = mipChain->image().extent();
            int thisWidth = vku::mipScale(chainExtent.width, i);
            int thisHeight = vku::mipScale(chainExtent.height, i);

            int nextWidth = vku::mipScale(chainExtent.width, i - 1);
            int nextHeight = vku::mipScale(chainExtent.height, i - 1);

            mipChain->image().barrier(cb,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
            // From this mip to next 
            vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, blurPipelineLayout, 0, 1, &chainToChainDS[i - 1], 0, nullptr);
            BloomBlurPC pushConstants{
                .direction = glm::vec2(0.5f, 0.0f),
                .inputMipLevel = (uint32_t)i,
                .resScalar = 1.0f,
                .resolution = glm::uvec2(nextWidth, nextHeight),
                .useUpsampleFilter = true
            };
            vkCmdPushConstants(cb, blurPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConstants), &pushConstants);
            vkCmdDispatch(cb, (nextWidth + 15) / 16, (nextHeight + 15) / 16, 1);
        }

        bloomTarget->image().setLayout(cb, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT);

        mipChain->image().setLayout(cb, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);

        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, applyPipeline);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, applyPipelineLayout, 0, 1, &applyDescriptorSet, 0, nullptr);
        vkCmdDispatch(cb, (ctx.passWidth + 15) / 16, (ctx.passHeight + 15) / 16, 1);

        bloomTarget->image().setLayout(cb, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);

        vkCmdEndDebugUtilsLabelEXT(ctx.cmdBuf);
    }

    BloomRenderPass::~BloomRenderPass() {
        delete mipChain;
    }
}
