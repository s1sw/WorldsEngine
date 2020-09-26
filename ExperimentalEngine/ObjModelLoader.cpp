#include "ObjModelLoader.hpp"
#include "mikktspace.h"
#include "tiny_obj_loader.h"
#include "weldmesh.h"
#include "Render.hpp"

namespace worlds {
    struct TangentCalcCtx {
        std::vector<Vertex>& verts;
        std::vector<uint32_t>& indices;
        std::vector<Vertex>& outVerts;
    };

    int getNumFaces(const SMikkTSpaceContext* ctx) {
        auto* tcc = (TangentCalcCtx*)ctx->m_pUserData;

        return tcc->indices.size() / 3;
    }

    // We only support loading triangles so don't worry about anything else
    int getNumVertsOfFace(const SMikkTSpaceContext*, const int) {
        return 3;
    }

    void getPosition(const SMikkTSpaceContext* ctx, float outPos[3], const int face, const int vertIdx) {
        auto* tcc = (TangentCalcCtx*)ctx->m_pUserData;

        int baseIndexIndex = face * 3;

        Vertex& vert = tcc->verts[tcc->indices[baseIndexIndex + vertIdx]];

        outPos[0] = vert.position.x;
        outPos[1] = vert.position.y;
        outPos[2] = vert.position.z;
    }

    void getNormal(const SMikkTSpaceContext* ctx, float outNorm[3], const int face, const int vertIdx) {
        auto* tcc = (TangentCalcCtx*)ctx->m_pUserData;

        int baseIndexIndex = face * 3;

        Vertex& vert = tcc->verts[tcc->indices[baseIndexIndex + vertIdx]];

        outNorm[0] = vert.normal.x;
        outNorm[1] = vert.normal.y;
        outNorm[2] = vert.normal.z;
    }

    void getTexCoord(const SMikkTSpaceContext* ctx, float outTC[2], const int face, const int vertIdx) {
        auto* tcc = (TangentCalcCtx*)ctx->m_pUserData;

        int baseIndexIndex = face * 3;

        Vertex& vert = tcc->verts[tcc->indices[baseIndexIndex + vertIdx]];

        outTC[0] = vert.uv.x;
        outTC[1] = vert.uv.y;
    }
    
    void setTSpace(const SMikkTSpaceContext* ctx, const float fvTangent[], const float fSign, const int iFace, const int iVert) {
        auto* tcc = (TangentCalcCtx*)ctx->m_pUserData;

        int baseIndexIndex = iFace * 3;

        Vertex& vert = tcc->verts[tcc->indices[baseIndexIndex + iVert]];
        vert.tangent = glm::vec3(fvTangent[0], fvTangent[1], fvTangent[2]);

        tcc->outVerts[(iFace * 3) + iVert] = vert;
    }

    void loadObj(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, std::istream& stream, LoadedMeshData& lmd) {
        indices.clear();
        vertices.clear();
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn;
        std::string err;

        tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, &stream);

        // Load the first shape
        size_t index_offset = 0;
        for (auto& idx : shapes[0].mesh.indices) {
            Vertex vert;
            vert.position = glm::vec3(attrib.vertices[3 * (size_t)idx.vertex_index], attrib.vertices[3 * (size_t)idx.vertex_index + 1], attrib.vertices[3 * (size_t)idx.vertex_index + 2]);
            vert.normal = glm::vec3(attrib.normals[3 * (size_t)idx.normal_index], attrib.normals[3 * (size_t)idx.normal_index + 1], attrib.normals[3 * (size_t)idx.normal_index + 2]);
            if (idx.texcoord_index >= 0)
                vert.uv = glm::vec2(1.0f - attrib.texcoords[2 * (size_t)idx.texcoord_index], 
                    attrib.texcoords[2 * (size_t)idx.texcoord_index + 1]);

            vertices.push_back(vert);
            indices.push_back((uint32_t)indices.size());
        }

        std::vector<Vertex> mikkTSpaceOut(shapes[0].mesh.indices.size());

        TangentCalcCtx tCalcCtx{ vertices, indices, mikkTSpaceOut };
        
        SMikkTSpaceInterface interface{};
        interface.m_getNumFaces = getNumFaces;
        interface.m_getNumVerticesOfFace = getNumVertsOfFace;
        interface.m_getPosition = getPosition;
        interface.m_getNormal = getNormal;
        interface.m_getTexCoord = getTexCoord;
        interface.m_setTSpaceBasic = setTSpace;

        SMikkTSpaceContext ctx;
        ctx.m_pInterface = &interface;
        ctx.m_pUserData = &tCalcCtx;

        genTangSpaceDefault(&ctx);

        std::vector<int> remapTable;
        remapTable.resize(mikkTSpaceOut.size());
        vertices.resize(mikkTSpaceOut.size());
        int finalVertCount = WeldMesh(remapTable.data(), (float*)vertices.data(), (float*)mikkTSpaceOut.data(), mikkTSpaceOut.size(), sizeof(Vertex) / sizeof(float));
        vertices.resize(finalVertCount);

        indices.clear();
        indices.reserve(mikkTSpaceOut.size());
        for (int i = 0; i < mikkTSpaceOut.size(); i++) {
            indices.push_back(remapTable[i]);
        }
    }
}