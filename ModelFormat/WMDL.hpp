#pragma once
#include <glm/glm.hpp>

namespace wmdl
{
    typedef uint32_t CountType;
    typedef uint64_t OffsetType;

    // Enforce tight packing for structs as they'll be saved to disk
#pragma pack(push, 1)
    struct Vertex
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 tangent;
        glm::vec2 uv;
        glm::vec2 uv2;
    };

    struct Vertex2
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 tangent;
        float bitangentSign;
        glm::vec2 uv;
        glm::vec2 uv2;
    };

    struct SubmeshInfo
    {
        CountType numVerts;
        CountType numIndices;
        OffsetType indexOffset;
        uint16_t materialIndex;

        void *getRelPtr(size_t offset)
        {
            return ((char *)this) + offset;
        }
    };

    struct SkinningInfoBlock
    {
        CountType numBones;
        OffsetType boneOffset;
        OffsetType skinningInfoOffset;
    };

    struct Bone
    {
        glm::mat4 inverseBindPose;
        glm::mat4 transform;
        char name[32] = {0};
        uint32_t parentBone = ~0u;

        void setName(const char *name)
        {
            int i = 0;
            for (; i < 32; i++)
            {
                this->name[i] = name[i];

                if (name[i] == '\0')
                    break;
            }

            for (; i < 32; i++)
            {
                this->name[i] = 0;
            }
        }
    };

    struct VertexSkinningInfo
    {
        uint32_t boneId[4];
        float boneWeight[4];
    };

    struct Header
    {
        char magic[4] = {'W', 'M', 'D', 'L'};
        int version = 3;
        bool useSmallIndices;
        CountType numSubmeshes;
        CountType numVertices;
        CountType numIndices;
        OffsetType submeshOffset;
        OffsetType indexOffset;
        OffsetType vertexOffset;

        bool verifyMagic()
        {
            return magic[0] == 'W' && magic[1] == 'M' && magic[2] == 'D' && magic[3] == 'L';
        }

        void *getRelPtr(size_t offset)
        {
            return ((char *)this) + offset;
        }

        SubmeshInfo *getSubmeshBlock()
        {
            return (SubmeshInfo *)getRelPtr(submeshOffset);
        }

        Vertex *getVertexBlock()
        {
            assert(version == 1);
            return (Vertex *)getRelPtr(vertexOffset);
        }

        Vertex2 *getVertex2Block()
        {
            assert(version >= 2);
            return (Vertex2 *)getRelPtr(vertexOffset);
        }

        SkinningInfoBlock *getSkinningInfoBlock()
        {
            assert(version >= 3);
            return (SkinningInfoBlock *)(this + 1);
        }

        Bone *getBones()
        {
            assert(version >= 3);
            return (Bone *)getRelPtr(getSkinningInfoBlock()->boneOffset);
        }

        VertexSkinningInfo *getVertexSkinningInfo()
        {
            assert(version >= 3);
            return (VertexSkinningInfo *)getRelPtr(getSkinningInfoBlock()->skinningInfoOffset);
        }

        bool isSkinned()
        {
            return version >= 3 && getSkinningInfoBlock()->numBones > 0;
        }

        uint32_t *getIndexBlock()
        {
            return (uint32_t *)getRelPtr(indexOffset);
        }
    };

#pragma pack(pop)
}
