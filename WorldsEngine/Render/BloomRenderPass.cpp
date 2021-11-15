#include "RenderPasses.hpp"
#include "ShaderReflector.hpp"
#include "ShaderCache.hpp"
#include "vku/DescriptorSetUtil.hpp":

namespace worlds {
    BloomRenderPass::BloomRenderPass(VulkanHandles* handles, RenderResource* hdrImg) : RenderPass(handles) {}

    void BloomRenderPass::setupApplyShader(RenderContext& ctx, VkDescriptorPool descriptorPool) {
        AssetID shaderId = AssetDB::pathToId("Shaders/bloom_apply.comp.spv");

        dsl = ShaderReflector{ shaderId }.createDescriptorSetLayout(handles->device, 0);

        vku::PipelineLayoutMaker plm;
        plm.descriptorSetLayout(dsl);
        bloomApplyPipelineLayout = plm.create(handles->device);

        vku::ComputePipelineMaker cpm;
        cpm.shader(VK_SHADER_STAGE_COMPUTE_BIT, ShaderCache::getModule(handles->device, shaderId));

        bloomApplyPipeline = cpm.create(handles->device, handles->pipelineCache, bloomApplyPipelineLayout);
        
        vku::SamplerMaker sm;
        sampler = sm.create(handles->device);

        vku::DescriptorSetMaker dsm;
        dsm.layout(dsl);
        descriptorSet = dsm.create(handles->device, descriptorPool)[0];
    }

    void BloomRenderPass::setup(RenderContext& ctx, VkDescriptorPool descriptorPool) {
        setupApplyShader(ctx, descriptorPool);

        VkExtent3D hdrExtent = hdrImg->image().extent();
        TextureResourceCreateInfo rci{
            TextureType::T2D,
            VK_FORMAT_B10G11R11_UFLOAT_PACK32,
            hdrExtent.width, hdrExtent.height,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
        };

        nMips = floor(log2(std::max(hdrExtent.width, hdrExtent.height)));
        rci.mipLevels = nMips;

        hdrImg = ctx.renderer->createTextureResource(rci, "Bloom mip chain");
    }
    
    void BloomRenderPass::execute(RenderContext& ctx) {
        addDebugLabel(ctx.cmdBuf, "Bloom Pass", 1.0f, 0.5f, 0.5f, 1.0f);
        VkCommandBuffer cb = ctx.cmdBuf;
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, bloomApplyPipeline);

        VkImageResolve resolve{};
        resolve.dstOffset = { 0, 0, 0 };
        resolve.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        resolve.srcOffset = { 0, 0, 0 };
        resolve.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        resolve.extent = hdrImg->image().extent();

        VkImageLayout initialLayout = hdrImg->image().layout();

        hdrImg->image().setLayout(cb, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        mipChain->image().setLayout(cb, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        vkCmdResolveImage(cb,
            hdrImg->image().image(), hdrImg->image().layout(),
            mipChain->image().image(), mipChain->image().layout(), 1, &resolve);

        hdrImg->image().setLayout(cb, initialLayout);

        for (int i = 1; i < nMips; i++) {
            VkExtent3D chainExtent = mipChain->image().extent();
            int thisWidth = vku::mipScale(chainExtent.width, i);
            int thisHeight = vku::mipScale(chainExtent.height, i);

            int prevWidth = vku::mipScale(chainExtent.width, i - 1);
            int prevHeight = vku::mipScale(chainExtent.height, i - 1);

            VkImageMemoryBarrier srcImb{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            srcImb.subresourceRange = VkImageSubresourceRange{
               .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
               .baseMipLevel = (uint32_t)i - 1,
               .levelCount = 1,
               .baseArrayLayer = 0,
               .layerCount = 1
            };

            srcImb.image = mipChain->image().image();
            srcImb.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            srcImb.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            srcImb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            srcImb.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

            vkCmdPipelineBarrier(cb,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_DEPENDENCY_BY_REGION_BIT,
                0, nullptr,
                0, nullptr,
                1, &srcImb
            );

            VkImageBlit ib{
                .srcSubresource = VkImageSubresourceLayers {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = (uint32_t)(i - 1),
                    .baseArrayLayer = 0,
                    .layerCount = 1
                },
                .srcOffsets = {
                    VkOffset3D { 0, 0, 0 },
                    VkOffset3D { prevWidth, prevHeight, 1 }
                },
                .dstSubresource = VkImageSubresourceLayers {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = (uint32_t)i,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                },
                .dstOffsets = {
                    VkOffset3D { 0, 0, 0 },
                    VkOffset3D { thisWidth, thisHeight, 1 }
                }
            };

            vkCmdBlitImage(cb,
                mipChain->image().image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                mipChain->image().image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &ib, VK_FILTER_LINEAR);
        }

        // Set the layout of the final mip as well
        VkImageMemoryBarrier srcImb{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        srcImb.subresourceRange = VkImageSubresourceRange{
           .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
           .baseMipLevel = (uint32_t)nMips,
           .levelCount = 1,
           .baseArrayLayer = 0,
           .layerCount = 1
        };

        srcImb.image = mipChain->image().image();
        srcImb.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        srcImb.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        srcImb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcImb.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(cb,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_DEPENDENCY_BY_REGION_BIT,
            0, nullptr,
            0, nullptr,
            1, &srcImb
        );

        mipChain->image().setCurrentLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);

        vkCmdEndDebugUtilsLabelEXT(ctx.cmdBuf);
    }

    BloomRenderPass::~BloomRenderPass() {}
}