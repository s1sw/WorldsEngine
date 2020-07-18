#pragma once
#include <unordered_set>
#include <functional>

namespace vk {
	enum class ImageAspectFlagBits;
}

struct TextureUsage;
struct RenderCtx;
struct PassSetupCtx;
struct ImageBarrier;
typedef uint32_t RenderImageHandle;

class RenderNode {
public:
	std::vector<TextureUsage> inputs;
	std::vector<TextureUsage> outputs;

	std::function<void()> setup;
	std::function<void(RenderCtx&)> execute;
};

struct RenderPassIO {
	std::vector<TextureUsage> inputs;
	std::vector<TextureUsage> outputs;
};

class RenderPass {
public:
	virtual RenderPassIO getIO() = 0;

	virtual void setup(PassSetupCtx& ctx, RenderCtx& rCtx) = 0;
	virtual void prePass(PassSetupCtx& ctx, RenderCtx& rCtx) {}
	virtual void execute(RenderCtx& ctx) = 0;
};

class GraphSolver {
public:
	GraphSolver();

	void addNode(RenderNode node) {
		rawNodes.push_back(node);
	}

	void clear() {
		rawNodes.clear();
	}

	std::vector<RenderNode> solve();
	std::vector<std::vector<ImageBarrier>> createImageBarriers(std::vector<RenderNode>& solvedNodes, std::unordered_map<RenderImageHandle, vk::ImageAspectFlagBits>& imageAspects);
private:
	std::vector<std::vector<int>> buildAdjacencyList();
	std::vector<RenderNode> topologicalSort(std::vector<std::vector<int>>& adjacencyList);
	std::vector<RenderNode> rawNodes;
};