#include "ModelCompiler.hpp"
#include "AssetCompilation/AssetCompilerUtil.hpp"
#include "Core/Log.hpp"
#include "IO/IOUtil.hpp"
#include "glm/gtc/type_ptr.hpp"
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
#include "robin_hood.h"
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

        glm::vec3 toGlm(aiVector3D v) {
            return glm::vec3{ v.x, v.y, v.z };
        }

        class PrintfStream : public LogStream {
        public:
            void write(const char* msg) override {
                printf("assimp: %s\n", msg);
            }
        };

        struct TangentCalcCtx {
            const std::vector<wmdl::Vertex2>& verts;
            std::vector<wmdl::Vertex2>& outVerts;
            const std::vector<wmdl::VertexSkinningInfo>& skinInfo;
            std::vector<wmdl::VertexSkinningInfo>& outSkinInfo;
        };

        void getPosition(const SMikkTSpaceContext* ctx, float outPos[3], const int face, const int vertIdx) {
            auto* tcc = (TangentCalcCtx*)ctx->m_pUserData;

            int baseIndexIndex = face * 3;

            const wmdl::Vertex2& vert = tcc->verts[baseIndexIndex + vertIdx];

            outPos[0] = vert.position.x;
            outPos[1] = vert.position.y;
            outPos[2] = vert.position.z;
        }

        void getNormal(const SMikkTSpaceContext* ctx, float outNorm[3], const int face, const int vertIdx) {
            auto* tcc = (TangentCalcCtx*)ctx->m_pUserData;

            int baseIndexIndex = face * 3;

            const wmdl::Vertex2& vert = tcc->verts[baseIndexIndex + vertIdx];

            outNorm[0] = vert.normal.x;
            outNorm[1] = vert.normal.y;
            outNorm[2] = vert.normal.z;
        }

        void getTexCoord(const SMikkTSpaceContext* ctx, float outTC[2], const int face, const int vertIdx) {
            auto* tcc = (TangentCalcCtx*)ctx->m_pUserData;

            int baseIndexIndex = face * 3;

            const wmdl::Vertex2& vert = tcc->verts[baseIndexIndex + vertIdx];

            outTC[0] = vert.uv.x;
            outTC[1] = vert.uv.y;
        }

        void setTSpace(const SMikkTSpaceContext* ctx, const float fvTangent[], const float fSign, const int iFace, const int iVert) {
            auto* tcc = (TangentCalcCtx*)ctx->m_pUserData;

            int baseIndexIndex = iFace * 3;

            wmdl::Vertex2 vert = tcc->verts[baseIndexIndex + iVert];
            vert.tangent = glm::vec3(fvTangent[0], fvTangent[1], fvTangent[2]);
            vert.bitangentSign = fSign;

            tcc->outVerts[(iFace * 3) + iVert] = vert;
            if (tcc->skinInfo.size() > 0)
                tcc->outSkinInfo[(iFace * 3) + iVert] = tcc->skinInfo[baseIndexIndex + iVert];
        }

        int getNumFaces(const SMikkTSpaceContext* ctx) {
            auto* tcc = (TangentCalcCtx*)ctx->m_pUserData;

            return tcc->verts.size() / 3;
        }

        // We only support loading triangles so don't worry about anything else
        int getNumVertsOfFace(const SMikkTSpaceContext*, const int) {
            return 3;
        }

        struct IntermediateBone {
            aiBone* bone;
            aiBone* parentBone;
        };

        struct Mesh {
            std::vector<wmdl::Vertex2> verts;
            std::vector<uint32_t> indices;
            std::vector<wmdl::VertexSkinningInfo> vertSkins;
            std::vector<aiBone*> bones;
            uint32_t indexOffsetInFile;
            uint32_t materialIdx;
        };


        glm::mat4 convMtx(const aiMatrix4x4& m4) {
            return glm::transpose(glm::make_mat4(&m4.a1));
        }

        Mesh processAiMesh(aiMesh* aiMesh, aiNode* node, aiMatrix4x4 hierarchyTransform) {
            bool hasBones = aiMesh->mNumBones > 0;

            Mesh mesh;
            mesh.materialIdx = aiMesh->mMaterialIndex;
            mesh.verts.reserve(mesh.verts.size() + aiMesh->mNumVertices);

            robin_hood::unordered_flat_map<std::string, uint32_t> boneIds;

            if (hasBones) {
                mesh.vertSkins.resize(aiMesh->mNumVertices);
                for (size_t i = 0; i < mesh.vertSkins.size(); i++) {
                    for (int j = 0; j < 4; j++) {
                        mesh.vertSkins[i].boneId[j] = 0;
                        mesh.vertSkins[i].boneWeight[j] = 0.0f;
                    }
                }
            }

            logMsg("Mesh %s has %i bones", aiMesh->mName.C_Str(), aiMesh->mNumBones);
            for (unsigned int i = 0; i < aiMesh->mNumBones; i++) {
                uint32_t boneIdx = 0;
                aiBone* aiBone = aiMesh->mBones[i];

                if (!boneIds.contains(aiBone->mName.C_Str())) {
                    boneIdx = mesh.bones.size();

                    mesh.bones.push_back(aiBone);
                    boneIds.insert({ aiBone->mName.C_Str(), boneIdx });
                } else {
                    boneIdx = boneIds[aiMesh->mBones[i]->mName.C_Str()];
                }

                for (uint32_t j = 0; j < aiBone->mNumWeights; j++) {
                    uint32_t vertexId = aiBone->mWeights[j].mVertexId;
                    float weight = aiBone->mWeights[j].mWeight;

                    if (weight == 0.0f) continue;

                    wmdl::VertexSkinningInfo& skinInfo = mesh.vertSkins[vertexId];

                    // Find the first bone index with 0 weight
                    uint32_t freeIdx = ~0u;

                    for (int k = 0; k < 4; k++) {
                        if (skinInfo.boneWeight[k] == 0.0f) {
                            freeIdx = k;
                            break;
                        }
                    }

                    if (freeIdx != ~0u) {
                        skinInfo.boneWeight[freeIdx] = weight;
                        skinInfo.boneId[freeIdx] = i;
                    }
                }
            }

            std::vector<wmdl::VertexSkinningInfo> vsi2;
            vsi2.reserve(aiMesh->mNumFaces * 3);

            for (unsigned int j = 0; j < aiMesh->mNumFaces; j++) {
                for (int k = 0; k < 3; k++) {
                    int idx = aiMesh->mFaces[j].mIndices[k];
                    wmdl::Vertex2 vtx;
                    vtx.position = convMtx(node->mTransformation * hierarchyTransform) * glm::vec4(toGlm(aiMesh->mVertices[idx]), 1.0f);
                    vtx.normal = glm::transpose(glm::inverse(convMtx(node->mTransformation * hierarchyTransform))) * glm::vec4(toGlm(aiMesh->mNormals[idx]), 0.0f);
                    //vtx.normal = toGlm(aiMesh->mNormals[idx]);
                    vtx.tangent = glm::vec3{ 0.0f };
                    vtx.bitangentSign = 0.0f;
                    vtx.uv = !aiMesh->HasTextureCoords(0) ? glm::vec2(0.0f) : toGlm(aiMesh->mTextureCoords[0][idx]);
                    mesh.verts.push_back(vtx);
                }
            }

            if (hasBones) {
                for (unsigned int j = 0; j < aiMesh->mNumFaces; j++) {
                    for (int k = 0; k < 3; k++) {
                        int idx = aiMesh->mFaces[j].mIndices[k];
                        vsi2.push_back(mesh.vertSkins[idx]);
                    }
                }
            }

            mesh.vertSkins = std::move(vsi2);

            std::vector<wmdl::Vertex2> mikkTSpaceOut(mesh.verts.size());
            std::vector<wmdl::VertexSkinningInfo> mikkTSpaceOutSkinInfo(mesh.vertSkins.size());
            TangentCalcCtx tCalcCtx{ mesh.verts, mikkTSpaceOut, mesh.vertSkins, mikkTSpaceOutSkinInfo };

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

            if (hasBones) {
                mesh.vertSkins.resize(finalVertCount);
                for (int i = 0; i < mikkTSpaceOutSkinInfo.size(); i++) {
                    mesh.vertSkins[remapTable[i]] = mikkTSpaceOutSkinInfo[i];
                }
            } else {
                mesh.vertSkins.resize(finalVertCount);
                for (int i = 0; i < mesh.vertSkins.size(); i++) {
                    for (int j = 0; j < 4; j++) {
                        mesh.vertSkins[i].boneId[j] = 0;
                        mesh.vertSkins[i].boneWeight[j] = j == 0 ? 1.0f : 0.0f;
                    }
                }
            }

            return mesh;
        }

        robin_hood::unordered_map<std::string, aiNode*> nodeLookup;

        void processNode(aiNode* node, std::vector<Mesh>& meshes, const aiScene* scene, aiMatrix4x4 parentTransform, int depth = 0) {
            char indentBuf[8] = { 0 };

            for (int i = 0; i < depth; i++) {
                indentBuf[i] = '\t';
            }

            logMsg("%s- %s", indentBuf, node->mName.C_Str());

            for (int i = 0; i < node->mNumMeshes; i++) {
                meshes.push_back(processAiMesh(scene->mMeshes[node->mMeshes[i]], node, parentTransform));
            }

            nodeLookup.insert({ node->mName.C_Str(), node });

            for (int i = 0; i < node->mNumChildren; i++) {
                processNode(node->mChildren[i], meshes, scene, node->mTransformation * parentTransform, depth + 1);
            }
        }

        ErrorCodes convertModel(AssetCompileOperation* compileOp, PHYSFS_File* outFile, void* data, size_t dataSize, const char* extension) {
            const int NUM_STEPS = 5;
            const float PROGRESS_PER_STEP = 1.0f / NUM_STEPS;
            DefaultLogger::get()->attachStream(new PrintfStream);
            nodeLookup.clear();

            Assimp::Importer importer;

            logMsg("Loading file...");

            const aiScene* scene = importer.ReadFileFromMemory(data, dataSize,
                aiProcess_CalcTangentSpace |
                aiProcess_GenSmoothNormals |
                aiProcess_JoinIdenticalVertices |
                aiProcess_ImproveCacheLocality |
                aiProcess_LimitBoneWeights |
                aiProcess_RemoveRedundantMaterials |
                aiProcess_Triangulate |
                aiProcess_GenUVCoords |
                aiProcess_SortByPType |
                aiProcess_FindDegenerates |
                aiProcess_FindInvalidData |
                aiProcess_FindInstances |
                aiProcess_ValidateDataStructure |
                aiProcess_OptimizeMeshes |
                aiProcess_OptimizeGraph |
                aiProcess_PopulateArmatureData |
                aiProcess_FlipUVs, extension);

            compileOp->progress = PROGRESS_PER_STEP;

            if (scene == nullptr) {
                logErr("Failed to import file: %s", importer.GetErrorString());
                compileOp->complete = true;
                return ErrorCodes::ImportFailure;
            }

            logMsg("Model importer: %i meshes:", scene->mNumMeshes);

            bool hasBones = false;

            for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
                auto mesh = scene->mMeshes[i];
                if (mesh->HasBones()) hasBones = true;
                logMsg("\t-%s: %i verts, %i tris, material index is %i", mesh->mName.C_Str(), mesh->mNumVertices, mesh->mNumFaces, mesh->mMaterialIndex);
            }

            logMsg("File has %u materials:", scene->mNumMaterials);
            for (unsigned int i = 0; i < scene->mNumMaterials; i++) {
                auto mat = scene->mMaterials[i];

                logMsg("\t-%s", mat->GetName().C_Str());
            }


            std::vector<Mesh> meshes;

            float perMeshProgress = PROGRESS_PER_STEP / scene->mNumMeshes;
            //for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
            //    compileOp->progress = PROGRESS_PER_STEP + (i * perMeshProgress);
            //    auto mesh = scene->mMeshes[i];
            //    meshes.push_back(processAiMesh(mesh));
            //}

            processNode(scene->mRootNode, meshes, scene, aiMatrix4x4{});

            compileOp->progress = PROGRESS_PER_STEP * 2;

            std::vector<wmdl::Vertex2> combinedVerts;
            std::vector<uint32_t> combinedIndices;

            std::vector<wmdl::Bone> combinedBones;
            robin_hood::unordered_map<std::string, uint32_t> combinedBoneIds;

            std::vector<wmdl::VertexSkinningInfo> combinedVertSkinningInfo;

            for (uint32_t i = 0; i < meshes.size(); i++) {
                compileOp->progress = (PROGRESS_PER_STEP * 2) + (perMeshProgress * i);
                auto& mesh = meshes[i];

                mesh.indexOffsetInFile = combinedIndices.size();

                for (auto& idx : mesh.indices) {
                    combinedIndices.push_back(idx + combinedVerts.size());
                }

                if (hasBones) {
                    for (auto& bone : mesh.bones) {
                        if (combinedBoneIds.contains(bone->mName.C_Str())) continue;

                        wmdl::Bone wBone;
                        wBone.setName(bone->mName.C_Str());
                        wBone.inverseBindPose = convMtx(bone->mOffsetMatrix);

                        aiNode* boneNode = nodeLookup.at(bone->mName.C_Str());
                        wBone.transform = convMtx(bone->mNode->mTransformation);

                        combinedBoneIds.insert({ wBone.name, combinedBones.size() });
                        combinedBones.push_back(wBone);
                    }

                    for (auto& bone : mesh.bones) {
                        aiNode* boneNode = bone->mNode;
                        aiNode* parentNode = boneNode->mParent;
                        uint32_t combinedId = combinedBoneIds.at(bone->mName.C_Str());

                        if (combinedBoneIds.contains(parentNode->mName.C_Str())) {
                            combinedBones[combinedId].parentBone = combinedBoneIds.at(parentNode->mName.C_Str());
                        }
                    }

                    int vertIdx = 0;
                    for (auto& skinInfo : mesh.vertSkins) {
                        for (int j = 0; j < 4; j++) {
                            if (skinInfo.boneWeight[j] == 0.0f) continue;
                            skinInfo.boneId[j] = combinedBoneIds[mesh.bones[skinInfo.boneId[j]]->mName.C_Str()];
                        }

                        combinedVertSkinningInfo.push_back(skinInfo);
                        vertIdx++;
                    }
                }

                for (auto& vtx : mesh.verts) {
                    combinedVerts.push_back(vtx);
                }
            }

            compileOp->progress = PROGRESS_PER_STEP * 3;

            wmdl::Header hdr;
            hdr.useSmallIndices = false;
            hdr.numSubmeshes = scene->mNumMeshes;
            hdr.submeshOffset = sizeof(hdr) + sizeof(wmdl::SkinningInfoBlock) +
                (sizeof(wmdl::VertexSkinningInfo) * combinedVertSkinningInfo.size()) + (sizeof(wmdl::Bone) * combinedBones.size());
            hdr.vertexOffset = hdr.submeshOffset + (sizeof(wmdl::SubmeshInfo) * meshes.size());
            hdr.indexOffset = hdr.vertexOffset + combinedVerts.size() * sizeof(wmdl::Vertex2);
            hdr.numVertices = combinedVerts.size();
            hdr.numIndices = combinedIndices.size();

            PHYSFS_writeBytes(outFile, &hdr, sizeof(hdr));

            wmdl::SkinningInfoBlock skinBlock;
            skinBlock.numBones = combinedBones.size();
            skinBlock.boneOffset = sizeof(hdr) + sizeof(skinBlock);
            skinBlock.skinningInfoOffset = skinBlock.boneOffset + (sizeof(wmdl::Bone) * combinedBones.size());

            PHYSFS_writeBytes(outFile, &skinBlock, sizeof(skinBlock));

            PHYSFS_writeBytes(outFile, combinedBones.data(), combinedBones.size() * sizeof(wmdl::Bone));
            PHYSFS_writeBytes(outFile, combinedVertSkinningInfo.data(), combinedVertSkinningInfo.size() * sizeof(wmdl::VertexSkinningInfo));

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
