#include "WMDLLoader.hpp"
#include <WMDL.hpp>

namespace worlds {
    void loadWorldsModel(AssetID wmdlId, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, LoadedMeshData& lmd) {
        PHYSFS_File* f = g_assetDB.openAssetFileRead(wmdlId);
        size_t fileSize = PHYSFS_fileLength(f);

        void* buf = malloc(fileSize);

        PHYSFS_readBytes(f, buf, fileSize);
        PHYSFS_close(f);
        
        wmdl::Header* wHdr = (wmdl::Header*)buf;

        logMsg("loading wmdl: %i submeshes", wHdr->numSubmeshes);

        wmdl::SubmeshInfo* submeshBlock = wHdr->getSubmeshBlock();
        lmd.numSubmeshes = wHdr->numSubmeshes;

        for (wmdl::CountType i = 0; i < wHdr->numSubmeshes; i++) {
            lmd.submeshes[i].indexCount = submeshBlock[i].numIndices;
            lmd.submeshes[i].indexOffset = submeshBlock[i].indexOffset;
        }

        vertices.resize(wHdr->numVertices);
        indices.resize(wHdr->numIndices);

        memcpy(vertices.data(), wHdr->getVertexBlock(), wHdr->numVertices * sizeof(wmdl::Vertex));
        memcpy(indices.data(), wHdr->getIndexBlock(), wHdr->numIndices * sizeof(uint32_t));
        free(buf);
    }
}
