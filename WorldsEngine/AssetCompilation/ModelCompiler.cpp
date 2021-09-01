#include "ModelCompiler.hpp"
#include "AssetCompilation/AssetCompilerUtil.hpp"
#include "Core/Log.hpp"
#include "IO/IOUtil.hpp"
#include "nlohmann/json.hpp"
#include <WMDL.hpp>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/Logger.hpp>
#include <assimp/LogStream.hpp>
#include <assimp/DefaultLogger.hpp>
#include <thread>
#include <vector>
#include "../Libs/mikktspace.h"
#include "../Libs/weldmesh.h"
#include <slib/Path.hpp>
#include <filesystem>
#define _CRT_SECURE_NO_WARNINGS

namespace worlds {
    enum class ErrorCodes {
        None,
        InvalidArgs = -1,
        ImportFailure = -2,
        MiscInternal = -3
    };

    namespace mc_internal {
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

    ErrorCodes convertModel(AssetCompileOperation* compileOp, PHYSFS_File* outFile, void* data, size_t dataSize, const char* extension) {
        const int NUM_STEPS = 5;
        const float PROGRESS_PER_STEP = 1.0f / NUM_STEPS;
        DefaultLogger::get()->attachStream(new PrintfStream);

        Assimp::Importer importer;

        logMsg("Loading file...");

        const aiScene* scene = importer.ReadFileFromMemory(data, dataSize,
                aiProcess_OptimizeMeshes |
                aiProcess_Triangulate |
                aiProcess_PreTransformVertices |
                aiProcess_JoinIdenticalVertices |
                aiProcess_FlipUVs, extension);

        compileOp->progress = PROGRESS_PER_STEP;

        if (scene == nullptr) {
            logErr("Failed to import file: %s", importer.GetErrorString());
            compileOp->complete = true;
            return ErrorCodes::ImportFailure;
        }

        logMsg("Model importer: %i meshes:", scene->mNumMeshes);

        for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
            auto mesh = scene->mMeshes[i];
            logMsg("\t-%s: %i verts, %i tris, material index is %i", mesh->mName.C_Str(), mesh->mNumVertices, mesh->mNumFaces, mesh->mMaterialIndex);
        }

        logMsg("File has %u materials:", scene->mNumMaterials);
        for (unsigned int i = 0; i < scene->mNumMaterials; i++) {
            auto mat = scene->mMaterials[i];

            logMsg("\t-%s", mat->GetName().C_Str());
        }


        std::vector<Mesh> meshes;

        float perMeshProgress = PROGRESS_PER_STEP / scene->mNumMeshes;
        for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
            compileOp->progress = PROGRESS_PER_STEP + (i * perMeshProgress);
            auto mesh = scene->mMeshes[i];
            meshes.push_back(processAiMesh(mesh));
        }

        compileOp->progress = PROGRESS_PER_STEP * 2;

        std::vector<wmdl::Vertex2> combinedVerts;
        std::vector<uint32_t> combinedIndices;
        for (uint32_t i = 0; i < meshes.size(); i++) {
            compileOp->progress = (PROGRESS_PER_STEP * 2) + (perMeshProgress * i);
            auto& mesh = meshes[i];

            mesh.indexOffsetInFile = combinedIndices.size();

            for (auto& idx : mesh.indices) {
                combinedIndices.push_back(idx + combinedVerts.size());
            }

            for (auto& vtx : mesh.verts) {
                combinedVerts.push_back(vtx);
            }
        }
        compileOp->progress = PROGRESS_PER_STEP * 3;

        wmdl::Header hdr;
        hdr.useSmallIndices = false;
        hdr.numSubmeshes = scene->mNumMeshes;
        hdr.submeshOffset = sizeof(hdr);
        hdr.vertexOffset = sizeof(hdr) + sizeof(wmdl::SubmeshInfo) * meshes.size();
        hdr.indexOffset = hdr.vertexOffset + combinedVerts.size() * sizeof(wmdl::Vertex2);
        hdr.numVertices = combinedVerts.size();
        hdr.numIndices = combinedIndices.size();

        PHYSFS_writeBytes(outFile, &hdr, sizeof(hdr));

        int i = 0;
        for (auto& mesh : meshes) {
            compileOp->progress = (PROGRESS_PER_STEP * 3) + (perMeshProgress * i);
            wmdl::SubmeshInfo submeshInfo;
            submeshInfo.numVerts = mesh.verts.size();
            submeshInfo.numIndices = mesh.indices.size();
            logMsg("post-process: mesh %i has %i verts, %i faces", i, (int)mesh.verts.size(), (int)mesh.indices.size() / 3);
            submeshInfo.materialIndex = mesh.materialIdx;
            submeshInfo.indexOffset = mesh.indexOffsetInFile;

            PHYSFS_writeBytes(outFile, &submeshInfo, sizeof(submeshInfo));
            i++;
        }

        compileOp->progress = PROGRESS_PER_STEP * 4;

        PHYSFS_writeBytes(outFile, combinedVerts.data(), sizeof(wmdl::Vertex2) * combinedVerts.size());
        PHYSFS_writeBytes(outFile, combinedIndices.data(), sizeof(uint32_t) * combinedIndices.size());

        compileOp->progress = 1.0f;
        compileOp->complete = true;

        return ErrorCodes::None;
    }
    }

    ModelCompiler::ModelCompiler() {
        AssetCompilers::registerCompiler(this);
    }

    AssetCompileOperation* ModelCompiler::compile(std::string_view projectRoot, AssetID src) {
        std::string inputPath = AssetDB::idToPath(src);
        auto jsonContents = LoadFileToString(inputPath);

        if (jsonContents.error != IOError::None) {
            logErr("Error opening asset file");
            return nullptr;
        }

        nlohmann::json j = nlohmann::json::parse(jsonContents.value);

        std::string outputPath = getOutputPath(AssetDB::idToPath(src));
        logMsg("Compiling %s to %s", AssetDB::idToPath(src).c_str(), outputPath.c_str());

        std::string modelSourcePath = j["srcPath"];
        slib::Path path(modelSourcePath.c_str());

        int64_t fileLen;
        auto result = LoadFileToBuffer(modelSourcePath, &fileLen);

        if (result.error != IOError::None) {
            logErr("Error opening source path %s", modelSourcePath.c_str());
            return nullptr;
        }

        AssetCompileOperation* compileOp = new AssetCompileOperation;
        compileOp->outputId = AssetDB::pathToId(outputPath);

        std::filesystem::path fullPath = projectRoot;
        fullPath /= outputPath;
        fullPath = fullPath.parent_path();
        fullPath = fullPath.lexically_normal();

        std::filesystem::create_directories(fullPath);

        std::thread([compileOp, outputPath, path, result, fileLen]() {
            PHYSFS_File* outFile = PHYSFS_openWrite(outputPath.c_str());
            slib::Path p = path;
            mc_internal::convertModel(compileOp, outFile, result.value, fileLen, p.fileExtension().cStr());
            PHYSFS_close(outFile);
        }).detach();

        return compileOp;
    }

    const char* ModelCompiler::getSourceExtension() {
        return ".wmdlj";
    }

    const char* ModelCompiler::getCompiledExtension() {
        return ".wmdl";
    }
}
