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
        sampler = sm.create(handles->device);

        vku::DescriptorSetMaker dsm;
        dsm.layout(applyDsl);
        applyDescriptorSet = dsm.create(handles->device, descriptorPool)[0];
    }

    struct BloomBlurPC {
        glm::vec2 direction;
        uint32_t inputMipLevel;
        uint32_t pad;
        glm::uvec2 resolution;
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
        for (int i = 0; i < nMips * 2; i++) {
            dsm.layout(blurDsl);
        }

        blurDescriptorSets = dsm.create(handles->device, handles->descriptorPool);

        vku::DescriptorSetUpdater dsu(0, nMips * 4, 0);
        for (int i = 0; i < nMips; i++) {
            dsu.beginDescriptorSet(blurDescriptorSets[(i * 2)]);
            dsu.beginImages(0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            dsu.image(sampler, mipChain->image().imageView(), VK_IMAGE_LAYOUT_GENERAL);
            dsu.beginImages(1, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
            dsu.image(sampler, blurIntermediate->image().imageView(), VK_IMAGE_LAYOUT_GENERAL);

            dsu.beginDescriptorSet(blurDescriptorSets[(i * 2) + 1]);
            dsu.beginImages(0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            dsu.image(sampler, blurIntermediate->image().imageView(), VK_IMAGE_LAYOUT_GENERAL);
            dsu.beginImages(1, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
            dsu.image(sampler, blurMipChainImageViews[i], VK_IMAGE_LAYOUT_GENERAL);
        }
        dsu.update(handles->device);
    }

    void BloomRenderPass::setup(RenderContext& ctx, VkDescriptorPool descriptorPool) {
        VkExtent3D hdrExtent = hdrImg->image().extent();
        TextureResourceCreateInfo rci{
            TextureType::T2D,
            VK_FORMAT_B10G11R11_UFLOAT_PACK32,
            (int)hdrExtent.width, (int)hdrExtent.height,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
        };

        nMips = floor(log2(std::max(hdrExtent.width, hdrExtent.height)));
        rci.mipLevels = nMips;

        mipChain = ctx.renderer->createTextureResource(rci, "Bloom mip chain");

        TextureResourceCreateInfo intermediateCi{
            TextureType::T2D,
            VK_FORMAT_B10G11R11_UFLOAT_PACK32,
            (int)hdrExtent.width, (int)hdrExtent.height,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
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

        VkImageResolve resolve{};
        resolve.dstOffset = { 0, 0, 0 };
        resolve.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        resolve.srcOffset = { 0, 0, 0 };
        resolve.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        resolve.extent = hdrImg->image().extent();

        VkImageLayout initialLayout = hdrImg->image().layout();

        hdrImg->image().setLayout(cb, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        mipChain->image().setLayout(cb, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);

        vkCmdResolveImage(cb,
            hdrImg->image().image(), hdrImg->image().layout(),
            mipChain->image().image(), mipChain->image().layout(), 1, &resolve);

        hdrImg->image().setLayout(cb, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);

        mipChain->image().setLayout(cb, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT);
        blurIntermediate->image().setLayout(cb, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT);

        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, blurPipeline);
        for (int i = 1; i < nMips; i++) {
            VkExtent3D chainExtent = mipChain->image().extent();
            int thisWidth = vku::mipScale(chainExtent.width, i);
            int thisHeight = vku::mipScale(chainExtent.height, i);

            int prevWidth = vku::mipScale(chainExtent.width, i - 1);
            int prevHeight = vku::mipScale(chainExtent.height, i - 1);
            vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, blurPipelineLayout, 0, 1, &blurDescriptorSets[(i * 2)], 0, nullptr);
            BloomBlurPC pushConstants{
                .direction = glm::vec2(3.0f, 0.0f),
                .inputMipLevel = (uint32_t)(i - 1),
                .resolution = glm::uvec2(thisWidth, thisHeight)
            };
            vkCmdPushConstants(cb, blurPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConstants), &pushConstants);
            vkCmdDispatch(cb, (thisWidth + 15) / 16, (thisHeight + 15) / 16, 1);

            vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, blurPipelineLayout, 0, 1, &blurDescriptorSets[(i * 2) + 1], 0, nullptr);
            pushConstants.direction = glm::vec2(0.0f, 3.0f);
            vkCmdPushConstants(cb, blurPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConstants), &pushConstants);
            vkCmdDispatch(cb, (thisWidth + 15) / 16, (thisHeight + 15) / 16, 1);

            //VkImageMemoryBarrier srcImb {};
            //transitionOneMip(srcImb, mipChain->image(), i - 1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            //    VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);

            //vkCmdPipelineBarrier(cb,
            //    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            //    VK_DEPENDENCY_BY_REGION_BIT,
            //    0, nullptr,
            //    0, nullptr,
            //    1, &srcImb
            //);

            //VkImageBlit ib{
            //    .srcSubresource = VkImageSubresourceLayers {
            //        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            //        .mipLevel = (uint32_t)(i - 1),
            //        .baseArrayLayer = 0,
            //        .layerCount = 1
            //    },
            //    .srcOffsets = {
            //        VkOffset3D { 0, 0, 0 },
            //        VkOffset3D { prevWidth, prevHeight, 1 }
            //    },
            //    .dstSubresource = VkImageSubresourceLayers {
            //        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            //        .mipLevel = (uint32_t)i,
            //        .baseArrayLayer = 0,
            //        .layerCount = 1
            //    },
            //    .dstOffsets = {
            //        VkOffset3D { 0, 0, 0 },
            //        VkOffset3D { thisWidth, thisHeight, 1 }
            //    }
            //};

            //vkCmdBlitImage(cb,
            //    mipChain->image().image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            //    mipChain->image().image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            //    1, &ib, VK_FILTER_NEAREST);
        }

        // Set the layout of the final mip as well
        //VkImageMemoryBarrier srcImb {};
        //transitionOneMip(srcImb, mipChain->image(), nMips - 1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        //    VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);

        //vkCmdPipelineBarrier(cb,
        //    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        //    VK_DEPENDENCY_BY_REGION_BIT,
        //    0, nullptr,
        //    0, nullptr,
        //    1, &srcImb
        //);

        bloomTarget->image().setLayout(cb, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT);
        mipChain->image().setCurrentLayout(VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

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
