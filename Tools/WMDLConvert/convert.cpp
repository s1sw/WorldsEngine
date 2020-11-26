#include <WMDL.hpp>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

enum ErrorCodes {
    Err_InvalidArgs = -1,
    Err_ImportFailure = -2
};

using namespace Assimp;

glm::vec3 toGlm (aiVector3D v) {
    return glm::vec3 { v.x, v.y, v.z };
}

int main(int argc, char** argv) {
    if (argc != 2)
        return Err_InvalidArgs;


    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(argv[1], aiProcess_OptimizeMeshes | aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_PreTransformVertices);

    if (scene == nullptr) {
        fprintf(stderr, "Failed to import file.\n");
        return Err_ImportFailure;
    }

    printf("Importing %s: %i meshes:\n", argv[1], scene->mNumMeshes);
    
    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        printf("\t-%s\n", scene->mMeshes[i]->mName.C_Str());
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

        currOffset += mesh->mNumFaces * 3 * sizeof(uint32_t);

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

    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        auto mesh = scene->mMeshes[i];
        std::vector<uint32_t> indices;
        indices.reserve(mesh->mNumFaces * 3);

        for (unsigned int j = 0; j < mesh->mNumFaces; j++) {
            for (int k = 0; k < 3; k++) {
                indices.push_back(mesh->mFaces[j].mIndices[k]);
            }
        }
        
        fwrite(indices.data(), sizeof(uint32_t), indices.size(), outputFile);
    }

    fclose(outputFile);
    return 0;
}
