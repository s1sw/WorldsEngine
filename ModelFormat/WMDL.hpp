#pragma once
#include <glm/glm.hpp>

namespace wmdl {
    typedef uint32_t CountType;
    typedef uint64_t OffsetType;

    // Enforce tight packing for structs as they'll be saved to disk 
#pragma pack(push, 1)
    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 tangent;
        glm::vec2 uv;
        glm::vec2 uv2;
    };

    struct SubmeshInfo {
        CountType numVerts;
        CountType numIndices;
        OffsetType indexOffset;
        uint16_t materialIndex;

        void* getRelPtr(size_t offset) {
            return ((char*)this) + offset;
        }
    };

    struct Header {
        char magic[4] = {'W', 'M', 'D', 'L'};
        int version = 1;
        bool useSmallIndices;
        CountType numSubmeshes;
        CountType numVertices;
        CountType numIndices;
        OffsetType submeshOffset;
        OffsetType indexOffset;
        OffsetType vertexOffset;

        bool verifyMagic() {
            return magic[0] == 'W' &&
                   magic[1] == 'M' &&
                   magic[2] == 'D' &&
                   magic[3] == 'L';
        }

        void* getRelPtr(size_t offset) {
            return ((char*)this) + offset;
        }

        SubmeshInfo* getSubmeshBlock() {
            return (SubmeshInfo*)getRelPtr(submeshOffset);
        }

        Vertex* getVertexBlock() {
            return (Vertex*)getRelPtr(vertexOffset);
        }

        uint32_t* getIndexBlock() {
            return (uint32_t*)getRelPtr(indexOffset);
        }
    };

#pragma pack(pop)
}
