#include "RenderGraph.hpp"
#include "Engine.hpp"

GraphSolver::GraphSolver() {

}

bool isNodeConnected(RenderNode& node1, RenderNode& node2) {
    for (auto& output : node1.outputs) {
        for (auto& input : node2.inputs) {
            if (output.handle == input.handle) {
                return true;
            }
        }
    }

    return false;
}

std::vector<RenderNode> GraphSolver::solve() {
    auto adjList = buildAdjacencyList();

    return topologicalSort(adjList);
}

std::vector<std::vector<ImageBarrier>> GraphSolver::createImageBarriers(std::vector<RenderNode>& solvedNodes, std::unordered_map<RenderImageHandle, vk::ImageAspectFlagBits>& imageAspects) {
    std::vector<std::vector<ImageBarrier>> imgBarriers;
    imgBarriers.resize(solvedNodes.size());

    struct UsageInfo {
        TextureUsage usage;
        int nodeIdx;

        bool operator < (UsageInfo& other) const {
            return nodeIdx < other.nodeIdx;
        }
    };

    // We only support writing to a texture once, but reading multiple times
    std::unordered_map<RenderImageHandle, UsageInfo> outputUsages;
    std::unordered_map<RenderImageHandle, std::vector<UsageInfo>> inputUsages;

    for (int i = 0; i < solvedNodes.size(); i++) {
        for (auto& output : solvedNodes[i].outputs) {
            outputUsages.insert({ output.handle, {output, i} });
        }

        for (auto& input : solvedNodes[i].inputs) {
            if (inputUsages.count(input.handle) == 0) {
                inputUsages.emplace(input.handle, std::vector<UsageInfo>());
            }

            auto& vec = inputUsages.at(input.handle);

            vec.push_back(UsageInfo{ input, i });
        }
    }

    for (auto& usagePair : inputUsages) {
        // Input usages is already sorted in order of first usage -> last usage
        TextureUsage lastUsage = outputUsages.at(usagePair.first).usage;

        for (auto& usageInf : usagePair.second) {
            auto& usage = usageInf.usage;
            imgBarriers[usageInf.nodeIdx].push_back({
                usage.handle,
                lastUsage.layout, usage.layout,
                imageAspects[usage.handle],
                lastUsage.accessFlags, usage.accessFlags,
                lastUsage.stageFlags, usage.stageFlags
                });
        }
    }

    return imgBarriers;
}

std::vector<std::vector<int>> GraphSolver::buildAdjacencyList() {
    std::vector<std::vector<int>> adjacencyList;

    for (int i = 0; i < rawNodes.size(); i++) {
        std::vector<int> connected;
        RenderNode& node = rawNodes[i];

        for (int j = 0; j < rawNodes.size(); j++) {
            if (i == j) continue;
            RenderNode& node2 = rawNodes[j];

            if (isNodeConnected(node, node2)) {
                connected.emplace_back(j);
            }
        }
        adjacencyList.emplace_back(connected);
    }

    return adjacencyList;
}

std::vector<RenderNode> GraphSolver::topologicalSort(std::vector<std::vector<int>>& adjacencyList) {
    std::vector<RenderNode> sorted;

    std::vector<bool> visited;
    std::vector<bool> onStack;

    visited.resize(adjacencyList.size());
    onStack.resize(adjacencyList.size());

    std::function<void(int)> searchFunc = [&](int nodeIdx) {
        if (visited[nodeIdx] && onStack[nodeIdx]) {
            throw std::runtime_error("Circular dependency detected");
        }

        if (visited[nodeIdx]) {
            return;
        }

        visited[nodeIdx] = true;

        auto& descendants = adjacencyList[nodeIdx];
        onStack[nodeIdx] = true;

        for (int i = 0; i < descendants.size(); i++) {
            searchFunc(descendants[i]);
        }

        sorted.push_back(rawNodes[nodeIdx]);

        onStack[nodeIdx] = false;
    };

    for (int i = 0; i < adjacencyList.size(); i++) {
        searchFunc(i);
    }

    std::reverse(sorted.begin(), sorted.end());

    return sorted;
}
