#include "RenderInternal.hpp"
#include <entt/entity/registry.hpp>

namespace worlds {
    SkinningMatricesManager::SkinningMatricesManager(VulkanHandles* handles, int framesInFlight) {
        for (int i = 0; i < framesInFlight; i++) {
            matrixBuffers.push_back(vku::GenericBuffer{
                handles->device, handles->allocator,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, sizeof(glm::mat4) * 512,
                VMA_MEMORY_USAGE_CPU_TO_GPU, "Skinning Matrices"
            });

            mappedBuffers.push_back(reinterpret_cast<glm::mat4*>(matrixBuffers[i].map(handles->device)));
        }
    }

    VkBuffer SkinningMatricesManager::getBuffer(int index) {
        return matrixBuffers[index].buffer();
    }

    vku::GenericBuffer* SkinningMatricesManager::getBuffers() {
        return matrixBuffers.data();
    }

    glm::mat4 getBoneTransform(LoadedMeshData& meshData, Pose& pose, int boneIdx) {
        glm::mat4 transform = pose.boneTransforms[boneIdx];

        uint32_t parentId = meshData.meshBones[boneIdx].parentIdx;

        while (parentId != ~0u) {
            transform = pose.boneTransforms[parentId] * transform;
            parentId = meshData.meshBones[parentId].parentIdx;
        }

        return transform;
    }

    void updateSkinningMatrices(LoadedMeshData& meshData, Pose& pose, glm::mat4* skinningMatricesMapped, int skinningOffset) {
        for (int i = 0; i < meshData.meshBones.size(); i++) {
            skinningMatricesMapped[i + skinningOffset] = getBoneTransform(meshData, pose, i) * meshData.meshBones[i].inverseBindPose;
        }
    }

    struct SkinningOffset {
        int offset;
    };

    void SkinningMatricesManager::updateBuffer(RenderContext& ctx) {
        int skinningOffset = 0;

        ctx.registry.view<Transform, SkinnedWorldObject>().each([&](entt::entity ent, Transform& t, SkinnedWorldObject& wo) {
            LoadedMeshData& lm = ctx.resources.meshes.at(wo.mesh);
            updateSkinningMatrices(lm, wo.currentPose, mappedBuffers[ctx.frameIndex], skinningOffset);
            ctx.registry.emplace_or_replace<SkinningOffset>(ent, skinningOffset);
            skinningOffset += lm.meshBones.size();
        });
    }
}