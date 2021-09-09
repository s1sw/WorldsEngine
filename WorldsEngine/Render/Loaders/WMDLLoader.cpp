#include "WMDLLoader.hpp"
#include <WMDL.hpp>

namespace worlds {
    void loadWorldsModel(AssetID wmdlId, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, LoadedMeshData& lmd) {
        PHYSFS_File* f = AssetDB::openAssetFileRead(wmdlId);
        size_t fileSize = PHYSFS_fileLength(f);

        void* buf = malloc(fileSize);

        PHYSFS_readBytes(f, buf, fileSize);
        PHYSFS_close(f);

        wmdl::Header* wHdr = (wmdl::Header*)buf;

        logVrb("loading wmdl: %i submeshes", wHdr->numSubmeshes);

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

        memcpy(indices.data(), wHdr->getIndexBlock(), wHdr->numIndices * sizeof(uint32_t));
        free(buf);
    }
}
