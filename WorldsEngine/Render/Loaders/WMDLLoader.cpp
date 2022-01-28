#include "WMDLLoader.hpp"
#include <WMDL.hpp>

namespace worlds {
    void loadWorldsModel(AssetID wmdlId, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, std::vector<VertSkinningInfo>& skinningInfos, LoadedMeshData& lmd) {
        PHYSFS_File* f = AssetDB::openAssetFileRead(wmdlId);
        size_t fileSize = PHYSFS_fileLength(f);

        void* buf = malloc(fileSize);

        PHYSFS_readBytes(f, buf, fileSize);
        PHYSFS_close(f);

        wmdl::Header* wHdr = (wmdl::Header*)buf;

        if (!wHdr->verifyMagic()) {
            logErr("Failed to load %s: invalid magic", AssetDB::idToPath(wmdlId).c_str());
            return;
        }

        logVrb("loading wmdl: %i submeshes", wHdr->numSubmeshes);

        lmd.isSkinned = wHdr->isSkinned();
        if (wHdr->isSkinned()) {
            wmdl::SkinningInfoBlock* skinInfoBlock = wHdr->getSkinningInfoBlock();
            logVrb("wmdl is skinned: %i bones", wHdr->getSkinningInfoBlock()->numBones);
            lmd.meshBones.resize(skinInfoBlock->numBones);

            wmdl::Bone* bones = wHdr->getBones();
            for (wmdl::CountType i = 0; i < skinInfoBlock->numBones; i++) {
                lmd.meshBones[i].inverseBindPose = bones[i].inverseBindPose;
                lmd.meshBones[i].transform = bones[i].transform;
                lmd.meshBones[i].parentIdx = bones[i].parentBone;
                lmd.meshBones[i].name = bones[i].name;
                lmd.boneUpdateOrder.push_back(i);
            }
        }

        //std::sort(lmd.boneUpdateOrder.begin(), lmd.boneUpdateOrder.end(), [&](uint32_t boneIdxA, uint32_t boneIdxB) {
        //    uint32_t parentIdxA = lmd.meshBones[boneIdxA].parentIdx;
        //    uint32_t parentIdxB = lmd.meshBones[boneIdxB].parentIdx;
        //
        //    if (parentIdxA == ~0u && parentIdxB != ~0u)
        //        return true;
        //    else if (parentIdxB == ~0u && parentIdxA != ~0u)
        //        return false;
        //
        //    return parentIdxA < boneIdxB;
        //});

        wmdl::SubmeshInfo* submeshBlock = wHdr->getSubmeshBlock();
        lmd.numSubmeshes = wHdr->numSubmeshes;
        if (lmd.numSubmeshes > NUM_SUBMESH_MATS) {
            logWarn("WMDL has more submeshes than possible");
            lmd.numSubmeshes = NUM_SUBMESH_MATS;
        }

        for (wmdl::CountType i = 0; i < lmd.numSubmeshes; i++) {
            lmd.submeshes[i].indexCount = submeshBlock[i].numIndices;
            lmd.submeshes[i].indexOffset = submeshBlock[i].indexOffset;
        }

        indices.resize(wHdr->numIndices);

        if (wHdr->version == 1) {
            vertices.reserve(wHdr->numVertices);

            for (wmdl::CountType i = 0; i < wHdr->numVertices; i++) {
                wmdl::Vertex v = wHdr->getVertexBlock()[i];
                vertices.emplace_back(Vertex {
                    .position = v.position,
                    .normal = v.normal,
                    .tangent = v.tangent,
                    .bitangentSign = 1.0f,
                    .uv = v.uv,
                    .uv2 = v.uv2
                });
            }
        } else {
            vertices.resize(wHdr->numVertices);

            memcpy(vertices.data(), wHdr->getVertex2Block(), wHdr->numVertices * sizeof(wmdl::Vertex2));
        }

        if (wHdr->isSkinned()) {
            skinningInfos.resize(wHdr->numVertices);
            memcpy(skinningInfos.data(), wHdr->getVertexSkinningInfo(), wHdr->numVertices * sizeof(wmdl::VertexSkinningInfo));
        }

        memcpy(indices.data(), wHdr->getIndexBlock(), wHdr->numIndices * sizeof(uint32_t));
        free(buf);
    }
}
