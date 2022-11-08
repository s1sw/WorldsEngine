#include "WMDLLoader.hpp"
#include <Core/Log.hpp>
#include <WMDL.hpp>

namespace worlds
{
    bool loadWorldsModel(AssetID wmdlId, LoadedMeshData& lmd)
    {
        PHYSFS_File* f = AssetDB::openAssetFileRead(wmdlId);

        if (f == nullptr)
        {
            return false;
        }

        size_t fileSize = PHYSFS_fileLength(f);

        void* buf = malloc(fileSize);

        PHYSFS_readBytes(f, buf, fileSize);
        PHYSFS_close(f);

        wmdl::Header* wHdr = (wmdl::Header*)buf;

        if (!wHdr->verifyMagic())
        {
            logErr("Failed to load %s: invalid magic", AssetDB::idToPath(wmdlId).c_str());
            return false;
        }

        logVrb("loading wmdl: %i submeshes, small indices: %i", wHdr->numSubmeshes, wHdr->useSmallIndices);

        lmd.isSkinned = wHdr->isSkinned();
        if (wHdr->isSkinned())
        {
            wmdl::SkinningInfoBlock* skinInfoBlock = wHdr->getSkinningInfoBlock();
            logVrb("wmdl is skinned: %i bones", wHdr->getSkinningInfoBlock()->numBones);
            lmd.bones.resize(skinInfoBlock->numBones);

            wmdl::Bone* bones = wHdr->getBones();
            for (wmdl::CountType i = 0; i < skinInfoBlock->numBones; i++)
            {
                lmd.bones[i].inverseBindPose = bones[i].inverseBindPose;
                lmd.bones[i].transform = bones[i].transform;
                lmd.bones[i].parentIdx = bones[i].parentBone;
                lmd.bones[i].name = bones[i].name;
            }
        }

        wmdl::SubmeshInfo* submeshBlock = wHdr->getSubmeshBlock();
        uint32_t numSubmeshes = wHdr->numSubmeshes;
        if (numSubmeshes > NUM_SUBMESH_MATS)
        {
            logWarn("WMDL has more submeshes than possible");
            numSubmeshes = NUM_SUBMESH_MATS;
        }

        lmd.submeshes.resize(numSubmeshes);

        for (wmdl::CountType i = 0; i < numSubmeshes; i++)
        {
            lmd.submeshes[i].indexCount = submeshBlock[i].numIndices;
            lmd.submeshes[i].indexOffset = submeshBlock[i].indexOffset;
            lmd.submeshes[i].materialIndex = submeshBlock[i].materialIndex;
            if (lmd.submeshes[i].materialIndex < 0 || lmd.submeshes[i].materialIndex > NUM_SUBMESH_MATS)
                lmd.submeshes[i].materialIndex = 0;
        }

        if (wHdr->useSmallIndices)
            lmd.indices16.resize(wHdr->numIndices);
        else
            lmd.indices32.resize(wHdr->numIndices);

        if (wHdr->version == 1)
        {
            lmd.vertices.reserve(wHdr->numVertices);

            for (wmdl::CountType i = 0; i < wHdr->numVertices; i++)
            {
                wmdl::Vertex v = wHdr->getVertexBlock()[i];
                lmd.vertices.emplace_back(Vertex{.position = v.position,
                                                 .normal = v.normal,
                                                 .tangent = v.tangent,
                                                 .bitangentSign = 1.0f,
                                                 .uv = v.uv,
                                                 .uv2 = v.uv2});
            }
        }
        else
        {
            lmd.vertices.resize(wHdr->numVertices);

            memcpy(lmd.vertices.data(), wHdr->getVertex2Block(), wHdr->numVertices * sizeof(wmdl::Vertex2));
        }

        if (wHdr->isSkinned())
        {
            lmd.skinningInfos.resize(wHdr->numVertices);
            memcpy(lmd.skinningInfos.data(), wHdr->getVertexSkinningInfo(),
                   wHdr->numVertices * sizeof(wmdl::VertexSkinningInfo));
        }

        if (wHdr->useSmallIndices)
        {
            lmd.indexType = IndexType::Uint16;
            memcpy(lmd.indices16.data(), wHdr->getIndexBlock(), wHdr->numIndices * sizeof(uint16_t));
        }
        else
        {
            lmd.indexType = IndexType::Uint32;
            memcpy(lmd.indices32.data(), wHdr->getIndexBlock(), wHdr->numIndices * sizeof(uint32_t));
        }

        free(buf);
        return true;
    }
}
