#include <WMDL.hpp>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/Logger.hpp>
#include <assimp/LogStream.hpp>
#include <assimp/DefaultLogger.hpp>
#include <vector>
#include "mikktspace.h"
#include "weldmesh.h"
#define _CRT_SECURE_NO_WARNINGS

enum ErrorCodes {
    Err_InvalidArgs = -1,
    Err_ImportFailure = -2,
    Err_MiscInternal = -3
};

using namespace Assimp;

glm::vec3 toGlm (aiVector3D v) {
    return glm::vec3 { v.x, v.y, v.z };
}

class PrintfStream : public LogStream {
public:
    void write(const char* msg) override {
        printf("assimp: %s\n", msg);
    }
};

struct TangentCalcCtx {
    std::vector<wmdl::Vertex2>& verts;
    std::vector<wmdl::Vertex2>& outVerts;
};

void getPosition(const SMikkTSpaceContext* ctx, float outPos[3], const int face, const int vertIdx) {
    auto* tcc = (TangentCalcCtx*)ctx->m_pUserData;

    int baseIndexIndex = face * 3;

    wmdl::Vertex2& vert = tcc->verts[baseIndexIndex + vertIdx];

    outPos[0] = vert.position.x;
    outPos[1] = vert.position.y;
    outPos[2] = vert.position.z;
}

void getNormal(const SMikkTSpaceContext* ctx, float outNorm[3], const int face, const int vertIdx) {
    auto* tcc = (TangentCalcCtx*)ctx->m_pUserData;

    int baseIndexIndex = face * 3;

    wmdl::Vertex2& vert = tcc->verts[baseIndexIndex + vertIdx];

    outNorm[0] = vert.normal.x;
    outNorm[1] = vert.normal.y;
    outNorm[2] = vert.normal.z;
}

void getTexCoord(const SMikkTSpaceContext* ctx, float outTC[2], const int face, const int vertIdx) {
    auto* tcc = (TangentCalcCtx*)ctx->m_pUserData;

    int baseIndexIndex = face * 3;

    wmdl::Vertex2& vert = tcc->verts[baseIndexIndex + vertIdx];

    outTC[0] = vert.uv.x;
    outTC[1] = vert.uv.y;
}

void setTSpace(const SMikkTSpaceContext* ctx, const float fvTangent[], const float fSign, const int iFace, const int iVert) {
    auto* tcc = (TangentCalcCtx*)ctx->m_pUserData;

    int baseIndexIndex = iFace * 3;

    wmdl::Vertex2& vert = tcc->verts[baseIndexIndex + iVert];
    vert.tangent = glm::vec3(fvTangent[0], fvTangent[1], fvTangent[2]);
    vert.bitangentSign = fSign;

    tcc->outVerts[(iFace * 3) + iVert] = vert;
}

int getNumFaces(const SMikkTSpaceContext* ctx) {
    auto* tcc = (TangentCalcCtx*)ctx->m_pUserData;

    return tcc->verts.size() / 3;
}

// We only support loading triangles so don't worry about anything else
int getNumVertsOfFace(const SMikkTSpaceContext*, const int) {
    return 3;
}

struct Mesh {
    std::vector<wmdl::Vertex2> verts;
    std::vector<uint32_t> indices;
    uint32_t indexOffsetInFile;
    uint32_t materialIdx;
};

Mesh processAiMesh(aiMesh* aiMesh) {
    Mesh mesh;
    mesh.materialIdx = aiMesh->mMaterialIndex;
    mesh.verts.reserve(mesh.verts.size() + aiMesh->mNumVertices);

    for (unsigned int j = 0; j < aiMesh->mNumFaces; j++) {
        for (int k = 0; k < 3; k++) {
            int idx = aiMesh->mFaces[j].mIndices[k];
            wmdl::Vertex2 vtx;
            vtx.position = toGlm(aiMesh->mVertices[idx]);
            vtx.normal = toGlm(aiMesh->mNormals[idx]);
            vtx.tangent = glm::vec3{0.0f};
            vtx.bitangentSign = 0.0f;
            vtx.uv = !aiMesh->HasTextureCoords(0) ? glm::vec2(0.0f) : toGlm(aiMesh->mTextureCoords[0][idx]);
            mesh.verts.push_back(vtx);
        }
    }

    std::vector<wmdl::Vertex2> mikkTSpaceOut(mesh.verts.size());
    TangentCalcCtx tCalcCtx{ mesh.verts, mikkTSpaceOut };

    SMikkTSpaceInterface interface {};
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
    mesh.verts.resize(mikkTSpaceOut.size());
    int finalVertCount = WeldMesh(remapTable.data(), (float*)mesh.verts.data(), (float*)mikkTSpaceOut.data(), mikkTSpaceOut.size(), sizeof(wmdl::Vertex2) / sizeof(float));
    mesh.verts.resize(finalVertCount);

    mesh.indices.reserve(mikkTSpaceOut.size());
    for (int i = 0; i < mikkTSpaceOut.size(); i++) {
        mesh.indices.push_back(remapTable[i]);
    }

    return mesh;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("usage:\n");
        printf("wmdl_convert.exe <model name>\n");
        printf("(that's it)\n");
        return Err_InvalidArgs;
    }
    DefaultLogger::get()->attachStream(new PrintfStream);

    Assimp::Importer importer;

    printf("Loading file...\n");

    const aiScene* scene = importer.ReadFile(argv[1],
            aiProcess_OptimizeMeshes
          | aiProcess_Triangulate
          | aiProcess_CalcTangentSpace
          | aiProcess_PreTransformVertices
          | aiProcess_JoinIdenticalVertices
          | aiProcess_FlipUVs);

    if (scene == nullptr) {
        fprintf(stderr, "Failed to import file: %s\n", importer.GetErrorString());
        return Err_ImportFailure;
    }

    printf("Importing %s: %i meshes:\n", argv[1], scene->mNumMeshes);

    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        auto mesh = scene->mMeshes[i];
        printf("\t-%s: %i verts, %i tris, material index is %i\n", mesh->mName.C_Str(), mesh->mNumVertices, mesh->mNumFaces, mesh->mMaterialIndex);
    }

    printf("File has %u materials:\n", scene->mNumMaterials);
    for (unsigned int i = 0; i < scene->mNumMaterials; i++) {
        auto mat = scene->mMaterials[i];

        printf("\t-%s\n", mat->GetName().C_Str());
    }


    std::vector<Mesh> meshes;

    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        auto mesh = scene->mMeshes[i];
        meshes.push_back(processAiMesh(mesh));
    }

    std::vector<wmdl::Vertex2> combinedVerts;
    std::vector<uint32_t> combinedIndices;
    uint32_t indexOffset = 0;
    for (auto& mesh : meshes) {
        for (auto& vtx : mesh.verts) {
            combinedVerts.push_back(vtx);
        }

        for (auto& idx : mesh.indices) {
            combinedIndices.push_back(idx + indexOffset);
        }

        mesh.indexOffsetInFile = indexOffset;
        indexOffset += mesh.indices.size();
    }

    FILE* outputFile = fopen("output.wmdl", "wb");
    wmdl::Header hdr;
    hdr.useSmallIndices = false;
    hdr.numSubmeshes = scene->mNumMeshes;
    hdr.submeshOffset = sizeof(hdr);
    hdr.vertexOffset = sizeof(hdr) + sizeof(wmdl::SubmeshInfo) * meshes.size();
    hdr.indexOffset = hdr.vertexOffset + combinedVerts.size() * sizeof(wmdl::Vertex2);
    hdr.numVertices = combinedVerts.size();
    hdr.numIndices = combinedIndices.size();

    fwrite(&hdr, sizeof(hdr), 1, outputFile);

    int i = 0;
    for (auto& mesh : meshes) {
        wmdl::SubmeshInfo submeshInfo;
        submeshInfo.numVerts = mesh.verts.size();
        submeshInfo.numIndices = mesh.indices.size();
        printf("post-process: mesh %i has %i verts, %i faces", i, (int)mesh.verts.size(), (int)mesh.indices.size() / 3);
        submeshInfo.materialIndex = mesh.materialIdx;
        submeshInfo.indexOffset = mesh.indexOffsetInFile;

        fwrite(&submeshInfo, sizeof(submeshInfo), 1, outputFile);
        i++;
    }

    fwrite(combinedVerts.data(), sizeof(wmdl::Vertex2), combinedVerts.size(), outputFile);
    fwrite(combinedIndices.data(), sizeof(uint32_t), combinedIndices.size(), outputFile);

    fclose(outputFile);
    return 0;
}
