#pragma once
#include "RenderGraph.hpp"
#include "vku/vku.hpp"
#include <glm/glm.hpp>

class PolyRenderPass : public RenderPass {
private:
	
	vk::UniqueRenderPass renderPass;
	vk::UniquePipeline pipeline;
	vk::UniquePipelineLayout pipelineLayout;
	vk::UniqueDescriptorSetLayout dsl;
	
	vk::UniquePipeline wireframePipeline;
	vk::UniquePipelineLayout wireframePipelineLayout;
	vk::UniqueDescriptorSetLayout wireframeDsl;

	vk::UniquePipeline pickingBufCsPipeline;
	vk::UniquePipelineLayout pickingBufCsLayout;
	vk::UniqueDescriptorSetLayout pickingBufCsDsl;
	vk::DescriptorSet pickingBufCsDs;

	vku::UniformBuffer vpUB;
	vku::UniformBuffer lightsUB;
	vku::UniformBuffer materialUB;
	vku::UniformBuffer modelMatrixUB;
	vku::GenericBuffer pickingBuffer;
	
	vku::ShaderModule fragmentShader;
	vku::ShaderModule vertexShader;

	vku::ShaderModule wireFragmentShader;
	vku::ShaderModule wireVertexShader;
	
	vk::UniqueSampler albedoSampler;
	vk::UniqueSampler shadowSampler;
	
	vk::UniqueFramebuffer renderFb;
	vk::DescriptorSet descriptorSet;
	vk::DescriptorSet wireframeDescriptorSet;
	
	RenderImageHandle depthStencilImage;
	RenderImageHandle polyImage;
	RenderImageHandle shadowImage;
	
	bool enablePicking;
	int pickX, pickY;
	uint32_t pickedEnt;
	vk::UniqueEvent pickEvent;
	bool pickThisFrame;
	bool awaitingResults;
	bool setEventNextFrame;

	void updateDescriptorSets(PassSetupCtx& ctx);
public:
	PolyRenderPass(RenderImageHandle depthStencilImage, RenderImageHandle polyImage, RenderImageHandle shadowImage, bool enablePicking = false);
	void setPickCoords(int x, int y) { pickX = x; pickY = y; }
	RenderPassIO getIO() override;
	void setup(PassSetupCtx& ctx) override;
	void prePass(PassSetupCtx& ctx, RenderCtx& rCtx) override;
	void execute(RenderCtx& ctx);
	void requestEntityPick();
	bool getPickedEnt(uint32_t* out);
	void lateUpdateVP(glm::mat4 views[2], glm::vec3 viewPos[2], vk::Device dev);
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
	vk::DescriptorSet rDescriptorSet;
	vk::UniqueSampler sampler;
	RenderImageHandle finalPrePresent;
	RenderImageHandle finalPrePresentR;
	RenderImageHandle hdrImg;
public:
	TonemapRenderPass(RenderImageHandle hdrImg, RenderImageHandle finalPrePresent);
	RenderPassIO getIO() override;
	void setup(PassSetupCtx& ctx) override;
	void execute(RenderCtx& ctx) override;
	void setRightFinalImage(PassSetupCtx& ctx, RenderImageHandle right);
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