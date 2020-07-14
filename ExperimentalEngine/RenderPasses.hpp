#pragma once
#include "RenderGraph.hpp"
#include "vku/vku.hpp"

class PolyRenderPass : public RenderPass {
private:
	vk::UniqueRenderPass renderPass;
	vk::UniquePipeline pipeline;
	vk::UniquePipelineLayout pipelineLayout;
	vk::UniqueDescriptorSetLayout dsl;
	vku::UniformBuffer vpUB;
	vku::UniformBuffer lightsUB;
	vku::UniformBuffer materialUB;
	vku::UniformBuffer modelMatrixUB;
	vku::ShaderModule fragmentShader;
	vku::ShaderModule vertexShader;
	vk::UniqueSampler albedoSampler;
	vk::UniqueSampler shadowSampler;
	vk::UniqueFramebuffer renderFb;
	vk::DescriptorSet descriptorSet;
	RenderImageHandle depthStencilImage;
	RenderImageHandle polyImage;
	RenderImageHandle shadowImage;
public:
	PolyRenderPass(RenderImageHandle depthStencilImage, RenderImageHandle polyImage, RenderImageHandle shadowImage);
	RenderPassIO getIO() override;
	void setup() override;
	void execute(RenderCtx& ctx);
};

class ShadowmapRenderPass : public RenderPass {
private:
	vk::UniqueRenderPass renderPass;
	vk::UniquePipeline pipeline;
	vk::UniquePipelineLayout pipelineLayout;
	vk::UniqueDescriptorSetLayout dsl;
	RenderImageHandle shadowImage;
	vk::UniqueFramebuffer shadowFb;
	vk::DescriptorSet descriptorSet;
public:
	ShadowmapRenderPass(RenderImageHandle shadowImage);
	RenderPassIO getIO() override;
	void setup() override;
	void execute(RenderCtx& ctx);
};

class TonemapRenderPass : public RenderPass {
private:
	vku::ShaderModule tonemapShader;
	vk::UniqueDescriptorSetLayout dsl;
	vk::UniquePipeline pipeline;
	vk::UniquePipelineLayout pipelineLayout;
	vk::DescriptorSet descriptorSet;
	RenderImageHandle finalPrePresent;
	RenderImageHandle hdrImg;
public:
	TonemapRenderPass(RenderImageHandle hdrImg, RenderImageHandle finalPrePresent);
	RenderPassIO getIO() override;
	void setup() override;
	void execute(RenderCtx& ctx) override;
};