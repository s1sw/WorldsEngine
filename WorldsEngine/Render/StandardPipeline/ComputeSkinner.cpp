#include "ComputeSkinner.hpp"
#include <Core/AssetDB.hpp>
#include <Core/MeshManager.hpp>
#include <entt/entity/registry.hpp>
#include <Render/RenderInternal.hpp>
#include <Render/SimpleCompute.hpp>
#include <R2/VK.hpp>

using namespace R2;

namespace worlds
{
    struct ComputeSkinnerPushConstants
    {
        uint32_t NumVertices;
        uint32_t PoseOffset;
        uint32_t InputOffset;
        uint32_t OutputOffset;
        uint32_t SkinInfoOffset;
    };

    ComputeSkinner::ComputeSkinner(VKRenderer* renderer)
        : renderer(renderer)
    {
        VK::BufferCreateInfo bci{ VK::BufferUsage::Storage, sizeof(glm::mat4) * 512, true };
        poseBuffer = renderer->getCore()->CreateBuffer(bci);

        cs = new SimpleCompute(renderer->getCore(), AssetDB::pathToId("Shaders/skinning.comp.spv"));
        cs->PushConstantSize(sizeof(ComputeSkinnerPushConstants));
        cs->BindStorageBuffer(0, renderer->getMeshManager()->getVertexBuffer());
        cs->BindStorageBuffer(1, renderer->getMeshManager()->getSkinInfoBuffer());
        cs->BindStorageBuffer(2, poseBuffer.Get());
        cs->Build();
    }

    glm::mat4 getBoneTransform(const LoadedMesh& meshData, Pose& pose, int boneIdx)
    {
        glm::mat4 transform = pose.boneTransforms[boneIdx];

        uint32_t parentId = meshData.bones[boneIdx].parentId;

        while (parentId != ~0u)
        {
            transform = pose.boneTransforms[parentId] * transform;
            parentId = meshData.bones[parentId].parentId;
        }

        return transform;
    }


    void ComputeSkinner::Execute(R2::VK::CommandBuffer& cb, entt::registry& reg)
    {
        cb.BeginDebugLabel("Compute Skinning", 0.561f, 0.192f, 0.004f);
        RenderMeshManager* rmm = renderer->getMeshManager();
        rmm->getVertexBuffer()->Acquire(cb, VK::AccessFlags::ShaderReadWrite, VK::PipelineStageFlags::ComputeShader);

        uint32_t matrixOffset = 0;
        glm::mat4* mappedPoses = (glm::mat4*)poseBuffer->Map();

        reg.view<SkinnedWorldObject, Transform>().each([&](SkinnedWorldObject& swo, Transform& t) {
            const LoadedMesh& lm = MeshManager::loadOrGet(swo.mesh);
            const RenderMeshInfo& rmi = renderer->getMeshManager()->loadOrGet(swo.mesh);

            for (int i = 0; i < lm.bones.size(); i++)
            {
                mappedPoses[matrixOffset + i] = getBoneTransform(lm, swo.currentPose, i) * lm.bones[i].inverseBindPose;
            }

            ComputeSkinnerPushConstants pcs{};
            pcs.NumVertices = rmi.numVertices;
            pcs.PoseOffset = matrixOffset;
            pcs.InputOffset = rmi.vertsOffset / sizeof(Vertex);
            pcs.OutputOffset = swo.skinnedVertexOffset + (renderer->getMeshManager()->getSkinnedVertsOffset() / sizeof(Vertex));
            pcs.SkinInfoOffset = rmi.skinInfoOffset / sizeof(VertexSkinInfo);
            cs->Dispatch(cb, pcs, (rmi.numVertices + 255) / 256, 1, 1);

            matrixOffset += lm.bones.size();
        });

        poseBuffer->Unmap();
        rmm->getVertexBuffer()->Acquire(cb, VK::AccessFlags::VertexAttributeRead, VK::PipelineStageFlags::VertexInput);
        cb.EndDebugLabel();
    }
}