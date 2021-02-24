#include <WMDL.hpp>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/Logger.hpp>
#include <assimp/LogStream.hpp>
#include <assimp/DefaultLogger.hpp>
#include <vector>
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

    FILE* outputFile = fopen("output.wmdl", "wb");
    wmdl::Header hdr;
    hdr.useSmallIndices = false;
    hdr.numSubmeshes = scene->mNumMeshes;
    hdr.submeshOffset = sizeof(hdr);
    hdr.vertexOffset = sizeof(hdr) + sizeof(wmdl::SubmeshInfo) * scene->mNumMeshes;
    hdr.indexOffset = hdr.vertexOffset;
    hdr.numVertices = 0;
    hdr.numIndices = 0;

    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        hdr.indexOffset += scene->mMeshes[i]->mNumVertices * sizeof(wmdl::Vertex);
        hdr.numVertices += scene->mMeshes[i]->mNumVertices;
        hdr.numIndices += scene->mMeshes[i]->mNumFaces * 3;
    }
    fwrite(&hdr, sizeof(hdr), 1, outputFile);

    size_t currOffset = 0;
    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        auto mesh = scene->mMeshes[i];

        wmdl::SubmeshInfo submeshInfo;
        submeshInfo.numVerts = mesh->mNumVertices;
        submeshInfo.numIndices = mesh->mNumFaces * 3;
        submeshInfo.materialIndex = mesh->mMaterialIndex;
        submeshInfo.indexOffset = currOffset;

        currOffset += mesh->mNumFaces * 3;

        fwrite(&submeshInfo, sizeof(submeshInfo), 1, outputFile);
    }

    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        auto mesh = scene->mMeshes[i];

        std::vector<wmdl::Vertex> verts;
        verts.reserve(mesh->mNumVertices);

        if (!mesh->HasTangentsAndBitangents()) {
            fprintf(stderr, "warning: mesh %s doesn't have tangents! make sure everything's uv unwrapped\n", mesh->mName.C_Str());
        }

        for (unsigned int j = 0; j < mesh->mNumVertices; j++) {
            wmdl::Vertex vtx;
            vtx.position = toGlm(mesh->mVertices[j]);
            vtx.normal = toGlm(mesh->mNormals[j]);
            if (mesh->HasTangentsAndBitangents())
                vtx.tangent = toGlm(mesh->mTangents[j]);
            vtx.uv = !mesh->HasTextureCoords(0) ? glm::vec2(0.0f) : toGlm(mesh->mTextureCoords[0][j]);
            verts.push_back(vtx);
        }

        fwrite(verts.data(), sizeof(wmdl::Vertex), verts.size(), outputFile);

    }

    uint32_t meshIdxOffset = 0;
    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        auto mesh = scene->mMeshes[i];
        std::vector<uint32_t> indices;
        indices.reserve(mesh->mNumFaces * 3);

        for (unsigned int j = 0; j < mesh->mNumFaces; j++) {
            for (int k = 0; k < 3; k++) {
                indices.push_back(mesh->mFaces[j].mIndices[k] + meshIdxOffset);
            }
        }

        meshIdxOffset += mesh->mNumVertices;
        fwrite(indices.data(), sizeof(uint32_t), indices.size(), outputFile);
    }

    fclose(outputFile);
    return 0;
}
