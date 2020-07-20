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
	void setup(PassSetupCtx& ctx) override;
	void prePass(PassSetupCtx& ctx, RenderCtx& rCtx) override;
	void execute(RenderCtx& ctx);
	virtual ~PolyRenderPass();
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
	vku::ShaderModule shadowVertexShader;
	vku::ShaderModule shadowFragmentShader;
	uint32_t shadowmapRes;
public:
	ShadowmapRenderPass(RenderImageHandle shadowImage);
	RenderPassIO getIO() override;
	void setup(PassSetupCtx& ctx) override;
	void execute(RenderCtx& ctx);
	virtual ~ShadowmapRenderPass() {}
};

class TonemapRenderPass : public RenderPass {
private:
	vku::ShaderModule tonemapShader;
	vk::UniqueDescriptorSetLayout dsl;
	vk::UniquePipeline pipeline;
	vk::UniquePipelineLayout pipelineLayout;
	vk::DescriptorSet descriptorSet;
	vk::UniqueSampler sampler;
	RenderImageHandle finalPrePresent;
	RenderImageHandle hdrImg;
	RenderImageHandle imguiImg;
public:
	TonemapRenderPass(RenderImageHandle hdrImg, RenderImageHandle imguiImg, RenderImageHandle finalPrePresent);
	RenderPassIO getIO() override;
	void setup(PassSetupCtx& ctx) override;
	void execute(RenderCtx& ctx) override;
	virtual ~TonemapRenderPass() {}
};

class ImGuiRenderPass : public RenderPass {
private:
	vk::UniqueRenderPass renderPass;
	vk::UniqueFramebuffer fb;
	RenderImageHandle target;
public:
	ImGuiRenderPass(RenderImageHandle imguiTarget);
	RenderPassIO getIO() override;
	void setup(PassSetupCtx& ctx) override;
	void execute(RenderCtx& ctx) override;
	virtual ~ImGuiRenderPass() {}
};