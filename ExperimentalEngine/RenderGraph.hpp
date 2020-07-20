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

struct RenderPassIO {
	std::vector<TextureUsage> inputs;
	std::vector<TextureUsage> outputs;
};

class RenderPass {
public:
	virtual RenderPassIO getIO() = 0;

	virtual void setup(PassSetupCtx& ctx) = 0;
	virtual void prePass(PassSetupCtx& ctx, RenderCtx& rCtx) {}
	virtual void execute(RenderCtx& ctx) = 0;
	virtual ~RenderPass() {}
};

class GraphSolver {
public:
	GraphSolver();

	void addNode(RenderPass* node) {
		rawNodes.push_back(node);
	}

	void clear() {
		for (auto* node : rawNodes) {
			delete node;
		}
		rawNodes.clear();
	}

	std::vector<RenderPass*> solve();
	std::vector<std::vector<ImageBarrier>> createImageBarriers(std::vector<RenderPass*>& solvedNodes, std::unordered_map<RenderImageHandle, vk::ImageAspectFlagBits>& imageAspects);
private:
	std::vector<std::vector<int>> buildAdjacencyList();
	std::vector<RenderPass*> topologicalSort(std::vector<std::vector<int>>& adjacencyList);
	std::vector<RenderPass*> rawNodes;
};