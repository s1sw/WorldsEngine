#include "ModelCompiler.hpp"
#include "AssetCompilation/AssetCompilerUtil.hpp"
#include "Core/Log.hpp"
#include "IO/IOUtil.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "nlohmann/json.hpp"
#define TINYGLTF_IMPLEMENTATION
#include "../Libs/mikktspace.h"
#include "../Libs/weldmesh.h"
#include "robin_hood.h"
#include <WMDL.hpp>
#include <assimp/DefaultLogger.hpp>
#include <assimp/Importer.hpp>
#include <assimp/LogStream.hpp>
#include <assimp/Logger.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <filesystem>
#include <optional>
#include <slib/Path.hpp>
#include <slib/Subprocess.hpp>
#include <thread>
#include <tiny_gltf.h>
#include <vector>
#define _CRT_SECURE_NO_WARNINGS

namespace worlds
{
    enum class ErrorCodes
    {
        None,
        InvalidArgs = -1,
        ImportFailure = -2,
        MiscInternal = -3
    };

    struct ConversionSettings
    {
        bool preTransformVerts = false;
        bool removeRedundantMaterials = true;
        float uniformScale = 1.0f;
    };

    namespace mc_internal
    {
        using namespace Assimp;

        glm::vec3 toGlm(aiVector3D v)
        {
            return glm::vec3{v.x, v.y, v.z};
        }

        class PrintfStream : public LogStream
        {
          public:
            void write(const char* msg) override
            {
                printf("assimp: %s\n", msg);
            }
        };

        struct TangentCalcCtx
        {
            const std::vector<wmdl::Vertex2>& verts;
            std::vector<wmdl::Vertex2>& outVerts;
            const std::vector<wmdl::VertexSkinningInfo>& skinInfo;
            std::vector<wmdl::VertexSkinningInfo>& outSkinInfo;
        };

        void getPosition(const SMikkTSpaceContext* ctx, float outPos[3], const int face, const int vertIdx)
        {
            auto* tcc = (TangentCalcCtx*)ctx->m_pUserData;

            int baseIndexIndex = face * 3;

            const wmdl::Vertex2& vert = tcc->verts[baseIndexIndex + vertIdx];

            outPos[0] = vert.position.x;
            outPos[1] = vert.position.y;
            outPos[2] = vert.position.z;
        }

        void getNormal(const SMikkTSpaceContext* ctx, float outNorm[3], const int face, const int vertIdx)
        {
            auto* tcc = (TangentCalcCtx*)ctx->m_pUserData;

            int baseIndexIndex = face * 3;

            const wmdl::Vertex2& vert = tcc->verts[baseIndexIndex + vertIdx];

            outNorm[0] = vert.normal.x;
            outNorm[1] = vert.normal.y;
            outNorm[2] = vert.normal.z;
        }

        void getTexCoord(const SMikkTSpaceContext* ctx, float outTC[2], const int face, const int vertIdx)
        {
            auto* tcc = (TangentCalcCtx*)ctx->m_pUserData;

            int baseIndexIndex = face * 3;

            const wmdl::Vertex2& vert = tcc->verts[baseIndexIndex + vertIdx];

            outTC[0] = vert.uv.x;
            outTC[1] = vert.uv.y;
        }

        void setTSpace(const SMikkTSpaceContext* ctx, const float fvTangent[], const float fSign, const int iFace,
                       const int iVert)
        {
            auto* tcc = (TangentCalcCtx*)ctx->m_pUserData;

            int baseIndexIndex = iFace * 3;

            wmdl::Vertex2 vert = tcc->verts[baseIndexIndex + iVert];
            vert.tangent = glm::vec3(fvTangent[0], fvTangent[1], fvTangent[2]);
            vert.bitangentSign = fSign;

            tcc->outVerts[(iFace * 3) + iVert] = vert;
            if (tcc->skinInfo.size() > 0)
                tcc->outSkinInfo[(iFace * 3) + iVert] = tcc->skinInfo[baseIndexIndex + iVert];
        }

        int getNumFaces(const SMikkTSpaceContext* ctx)
        {
            auto* tcc = (TangentCalcCtx*)ctx->m_pUserData;

            return tcc->verts.size() / 3;
        }

        // We only support loading triangles so don't worry about anything else
        int getNumVertsOfFace(const SMikkTSpaceContext*, const int)
        {
            return 3;
        }

        struct IntermediateBone
        {
            aiBone* bone;
            aiBone* parentBone;
        };

        struct Mesh
        {
            std::vector<wmdl::Vertex2> verts;
            std::vector<uint32_t> indices;
            std::vector<wmdl::VertexSkinningInfo> vertSkins;
            std::vector<aiBone*> bones;
            uint32_t indexOffsetInFile;
            uint32_t materialIdx;
        };

        glm::mat4 convMtx(const aiMatrix4x4& aiMat)
        {
            return {aiMat.a1, aiMat.b1, aiMat.c1, aiMat.d1, aiMat.a2, aiMat.b2, aiMat.c2, aiMat.d2,
                    aiMat.a3, aiMat.b3, aiMat.c3, aiMat.d3, aiMat.a4, aiMat.b4, aiMat.c4, aiMat.d4};
        }

        Mesh processAiMesh(aiMesh* aiMesh, aiNode* node, aiMatrix4x4 hierarchyTransform)
        {
            bool hasBones = aiMesh->mNumBones > 0;

            Mesh mesh;
            mesh.materialIdx = aiMesh->mMaterialIndex;
            mesh.verts.reserve(mesh.verts.size() + aiMesh->mNumVertices);

            robin_hood::unordered_flat_map<std::string, uint32_t> boneIds;

            if (hasBones)
            {
                mesh.vertSkins.resize(aiMesh->mNumVertices);
                for (size_t i = 0; i < mesh.vertSkins.size(); i++)
                {
                    for (int j = 0; j < 4; j++)
                    {
                        mesh.vertSkins[i].boneId[j] = 0;
                        mesh.vertSkins[i].boneWeight[j] = 0.0f;
                    }
                }
            }

            for (unsigned int i = 0; i < aiMesh->mNumBones; i++)
            {
                uint32_t boneIdx = 0;
                aiBone* aiBone = aiMesh->mBones[i];

                if (!boneIds.contains(aiBone->mName.C_Str()))
                {
                    boneIdx = mesh.bones.size();

                    mesh.bones.push_back(aiBone);
                    boneIds.insert({aiBone->mName.C_Str(), boneIdx});
                }
                else
                {
                    boneIdx = boneIds[aiMesh->mBones[i]->mName.C_Str()];
                }

                for (uint32_t j = 0; j < aiBone->mNumWeights; j++)
                {
                    uint32_t vertexId = aiBone->mWeights[j].mVertexId;
                    float weight = aiBone->mWeights[j].mWeight;

                    if (weight == 0.0f)
                        continue;

                    wmdl::VertexSkinningInfo& skinInfo = mesh.vertSkins[vertexId];

                    // Find the first bone index with 0 weight
                    uint32_t freeIdx = ~0u;

                    for (int k = 0; k < 4; k++)
                    {
                        if (skinInfo.boneWeight[k] == 0.0f)
                        {
                            freeIdx = k;
                            break;
                        }
                    }

                    if (freeIdx != ~0u)
                    {
                        skinInfo.boneWeight[freeIdx] = weight;
                        skinInfo.boneId[freeIdx] = i;
                    }
                }
            }

            std::vector<wmdl::VertexSkinningInfo> vsi2;
            vsi2.reserve(aiMesh->mNumFaces * 3);

            for (unsigned int j = 0; j < aiMesh->mNumFaces; j++)
            {
                for (int k = 0; k < 3; k++)
                {
                    int idx = aiMesh->mFaces[j].mIndices[k];
                    wmdl::Vertex2 vtx;
                    vtx.position = convMtx(node->mTransformation * hierarchyTransform) *
                                   glm::vec4(toGlm(aiMesh->mVertices[idx]), 1.0f);
                    vtx.normal = glm::transpose(glm::inverse(convMtx(node->mTransformation * hierarchyTransform))) *
                                 glm::vec4(toGlm(aiMesh->mNormals[idx]), 0.0f);
                    // vtx.normal = toGlm(aiMesh->mNormals[idx]);
                    vtx.tangent = glm::vec3{0.0f};
                    vtx.bitangentSign = 0.0f;
                    vtx.uv = !aiMesh->HasTextureCoords(0) ? glm::vec2(0.0f) : toGlm(aiMesh->mTextureCoords[0][idx]);
                    mesh.verts.push_back(vtx);
                }
            }

            if (hasBones)
            {
                for (unsigned int j = 0; j < aiMesh->mNumFaces; j++)
                {
                    for (int k = 0; k < 3; k++)
                    {
                        int idx = aiMesh->mFaces[j].mIndices[k];
                        vsi2.push_back(mesh.vertSkins[idx]);
                    }
                }
            }

            mesh.vertSkins = std::move(vsi2);

            std::vector<wmdl::Vertex2> mikkTSpaceOut(mesh.verts.size());
            std::vector<wmdl::VertexSkinningInfo> mikkTSpaceOutSkinInfo(mesh.vertSkins.size());
            TangentCalcCtx tCalcCtx{mesh.verts, mikkTSpaceOut, mesh.vertSkins, mikkTSpaceOutSkinInfo};

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
            mesh.verts.resize(mikkTSpaceOut.size());
            int finalVertCount = WeldMesh(remapTable.data(), (float*)mesh.verts.data(), (float*)mikkTSpaceOut.data(),
                                          mikkTSpaceOut.size(), sizeof(wmdl::Vertex2) / sizeof(float));
            mesh.verts.resize(finalVertCount);

            mesh.indices.reserve(mikkTSpaceOut.size());
            for (int i = 0; i < mikkTSpaceOut.size(); i++)
            {
                mesh.indices.push_back(remapTable[i]);
            }

            if (hasBones)
            {
                mesh.vertSkins.resize(finalVertCount);
                for (int i = 0; i < mikkTSpaceOutSkinInfo.size(); i++)
                {
                    mesh.vertSkins[remapTable[i]] = mikkTSpaceOutSkinInfo[i];
                }
            }
            else
            {
                mesh.vertSkins.resize(finalVertCount);
                for (int i = 0; i < mesh.vertSkins.size(); i++)
                {
                    for (int j = 0; j < 4; j++)
                    {
                        mesh.vertSkins[i].boneId[j] = 0;
                        mesh.vertSkins[i].boneWeight[j] = j == 0 ? 1.0f : 0.0f;
                    }
                }
            }

            return mesh;
        }

        robin_hood::unordered_map<std::string, aiNode*> nodeLookup;

        void processNode(aiNode* node, std::vector<Mesh>& meshes, const aiScene* scene, aiMatrix4x4 parentTransform,
                         int depth = 0)
        {
            char indentBuf[16] = {0};

            for (int i = 0; i < std::min(depth, 15); i++)
            {
                indentBuf[i] = '\t';
            }

            logMsg("%s- %s", indentBuf, node->mName.C_Str());

            for (int i = 0; i < node->mNumMeshes; i++)
            {
                aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
                if (mesh->mPrimitiveTypes != aiPrimitiveType_TRIANGLE)
                    continue;
                meshes.push_back(processAiMesh(mesh, node, parentTransform));
            }

            nodeLookup.insert({node->mName.C_Str(), node});

            for (int i = 0; i < node->mNumChildren; i++)
            {
                processNode(node->mChildren[i], meshes, scene, node->mTransformation * parentTransform, depth + 1);
            }
        }

        ErrorCodes convertAssimpModel(AssetCompileOperation* compileOp, PHYSFS_File* outFile, void* data,
                                      size_t dataSize, const char* extension, ConversionSettings settings)
        {
            const int NUM_STEPS = 5;
            const float PROGRESS_PER_STEP = 1.0f / NUM_STEPS;
            DefaultLogger::get()->attachStream(new PrintfStream);
            nodeLookup.clear();

            Assimp::Importer importer;

            logMsg("Loading file...");

            uint32_t processFlags =
                aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals | aiProcess_JoinIdenticalVertices |
                aiProcess_ImproveCacheLocality | aiProcess_LimitBoneWeights | aiProcess_Triangulate |
                aiProcess_GenUVCoords | aiProcess_SortByPType | aiProcess_FindDegenerates | aiProcess_FindInvalidData |
                aiProcess_FindInstances | aiProcess_ValidateDataStructure | aiProcess_OptimizeMeshes |
                aiProcess_OptimizeGraph | aiProcess_PopulateArmatureData | aiProcess_GlobalScale | aiProcess_FlipUVs;

            importer.SetPropertyFloat(AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY, settings.uniformScale);

            if (settings.preTransformVerts)
                processFlags |= aiProcess_PreTransformVertices;

            if (settings.removeRedundantMaterials)
                processFlags |= aiProcess_RemoveRedundantMaterials;
            const aiScene* scene = importer.ReadFileFromMemory(data, dataSize, processFlags, extension);

            compileOp->progress = PROGRESS_PER_STEP;

            if (scene == nullptr)
            {
                logErr("Failed to import file: %s", importer.GetErrorString());
                compileOp->complete = true;
                compileOp->result = CompilationResult::Error;
                return ErrorCodes::ImportFailure;
            }

            logMsg("Model importer: %i meshes:", scene->mNumMeshes);

            bool hasBones = false;

            for (unsigned int i = 0; i < scene->mNumMeshes; i++)
            {
                auto mesh = scene->mMeshes[i];
                if (mesh->HasBones())
                    hasBones = true;
                logMsg("\t-%s: %i verts, %i tris, material index is %i", mesh->mName.C_Str(), mesh->mNumVertices,
                       mesh->mNumFaces, mesh->mMaterialIndex);
            }

            logMsg("File has %u materials:", scene->mNumMaterials);
            for (unsigned int i = 0; i < scene->mNumMaterials; i++)
            {
                auto mat = scene->mMaterials[i];

                logMsg("\t-%s", mat->GetName().C_Str());
            }

            std::vector<Mesh> meshes;

            float perMeshProgress = PROGRESS_PER_STEP / scene->mNumMeshes;
            // for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
            //    compileOp->progress = PROGRESS_PER_STEP + (i * perMeshProgress);
            //    auto mesh = scene->mMeshes[i];
            //    meshes.push_back(processAiMesh(mesh));
            //}

            processNode(scene->mRootNode, meshes, scene, aiMatrix4x4{});

            compileOp->progress = PROGRESS_PER_STEP * 2;

            // Merge meshes with the same material
            if (!hasBones)
            {
                robin_hood::unordered_map<uint32_t, std::vector<uint32_t>> materialMeshes;
                uint32_t meshIndex = 0;
                for (Mesh& m : meshes)
                {
                    if (!materialMeshes.contains(m.materialIdx))
                    {
                        materialMeshes.insert({m.materialIdx, std::vector<uint32_t>()});
                    }
                    materialMeshes[m.materialIdx].push_back(meshIndex);
                    meshIndex++;
                }

                std::vector<Mesh> oldMeshes;
                oldMeshes.swap(meshes);

                for (auto& pair : materialMeshes)
                {
                    Mesh newMesh;
                    newMesh.materialIdx = pair.first;
                    for (uint32_t mIdx : pair.second)
                    {
                        Mesh& m = oldMeshes[mIdx];

                        newMesh.indices.reserve(newMesh.indices.size() + m.indices.size());
                        for (uint32_t idx : m.indices)
                        {
                            newMesh.indices.push_back(idx + newMesh.verts.size());
                        }
                        newMesh.verts.insert(newMesh.verts.end(), m.verts.begin(), m.verts.end());
                    }
                    meshes.push_back(std::move(newMesh));
                }
            }

            std::vector<wmdl::Vertex2> combinedVerts;
            std::vector<uint32_t> combinedIndices;

            std::vector<wmdl::Bone> combinedBones;
            robin_hood::unordered_map<std::string, uint32_t> combinedBoneIds;

            std::vector<wmdl::VertexSkinningInfo> combinedVertSkinningInfo;

            for (uint32_t i = 0; i < meshes.size(); i++)
            {
                compileOp->progress = (PROGRESS_PER_STEP * 2) + (perMeshProgress * i);
                auto& mesh = meshes[i];

                mesh.indexOffsetInFile = combinedIndices.size();

                for (auto& idx : mesh.indices)
                {
                    combinedIndices.push_back(idx + combinedVerts.size());
                }

                if (hasBones)
                {
                    for (auto& bone : mesh.bones)
                    {
                        if (combinedBoneIds.contains(bone->mName.C_Str()))
                            continue;

                        wmdl::Bone wBone;
                        wBone.setName(bone->mName.C_Str());
                        wBone.inverseBindPose = convMtx(bone->mOffsetMatrix);

                        aiNode* boneNode = nodeLookup.at(bone->mName.C_Str());
                        wBone.transform = convMtx(bone->mNode->mTransformation);

                        combinedBoneIds.insert({wBone.name, combinedBones.size()});
                        combinedBones.push_back(wBone);
                    }

                    for (auto& bone : mesh.bones)
                    {
                        aiNode* boneNode = bone->mNode;
                        aiNode* parentNode = boneNode->mParent;
                        uint32_t combinedId = combinedBoneIds.at(bone->mName.C_Str());

                        if (combinedBoneIds.contains(parentNode->mName.C_Str()))
                        {
                            combinedBones[combinedId].parentBone = combinedBoneIds.at(parentNode->mName.C_Str());
                        }
                    }

                    int vertIdx = 0;
                    for (auto& skinInfo : mesh.vertSkins)
                    {
                        for (int j = 0; j < 4; j++)
                        {
                            if (skinInfo.boneWeight[j] == 0.0f)
                                continue;
                            skinInfo.boneId[j] = combinedBoneIds[mesh.bones[skinInfo.boneId[j]]->mName.C_Str()];
                        }

                        combinedVertSkinningInfo.push_back(skinInfo);
                        vertIdx++;
                    }
                }

                for (auto& vtx : mesh.verts)
                {
                    combinedVerts.push_back(vtx);
                }
            }

            compileOp->progress = PROGRESS_PER_STEP * 3;

            wmdl::Header hdr;
            hdr.useSmallIndices = combinedVerts.size() >= UINT16_MAX;
            hdr.numSubmeshes = meshes.size();
            hdr.submeshOffset = sizeof(hdr) + sizeof(wmdl::SkinningInfoBlock) +
                                (sizeof(wmdl::VertexSkinningInfo) * combinedVertSkinningInfo.size()) +
                                (sizeof(wmdl::Bone) * combinedBones.size());
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
            PHYSFS_writeBytes(outFile, combinedVertSkinningInfo.data(),
                              combinedVertSkinningInfo.size() * sizeof(wmdl::VertexSkinningInfo));

            int i = 0;
            for (auto& mesh : meshes)
            {
                compileOp->progress = (PROGRESS_PER_STEP * 3) + (perMeshProgress * i);
                wmdl::SubmeshInfo submeshInfo;
                submeshInfo.numVerts = mesh.verts.size();
                submeshInfo.numIndices = mesh.indices.size();
                logMsg("post-process: mesh %i has %i verts, %i faces", i, (int)mesh.verts.size(),
                       (int)mesh.indices.size() / 3);
                submeshInfo.materialIndex = mesh.materialIdx;
                submeshInfo.indexOffset = mesh.indexOffsetInFile;

                PHYSFS_writeBytes(outFile, &submeshInfo, sizeof(submeshInfo));
                i++;
            }

            compileOp->progress = PROGRESS_PER_STEP * 4;

            PHYSFS_writeBytes(outFile, combinedVerts.data(), sizeof(wmdl::Vertex2) * combinedVerts.size());
            if (hdr.useSmallIndices)
            {
                std::vector<uint16_t> smallIndices;
                smallIndices.resize(combinedIndices.size());

                for (size_t i = 0; i < combinedIndices.size(); i++)
                {
                    smallIndices[i] = (uint16_t)combinedIndices[i];
                }

                PHYSFS_writeBytes(outFile, smallIndices.data(), sizeof(uint16_t) * smallIndices.size());
            }
            else
            {
                PHYSFS_writeBytes(outFile, combinedIndices.data(), sizeof(uint32_t) * combinedIndices.size());
            }

            compileOp->progress = 1.0f;
            compileOp->complete = true;
            compileOp->result = CompilationResult::Success;

            return ErrorCodes::None;
        }

        class GltfModelConverter
        {
          private:
            struct ConvertedGltfNode
            {
                glm::mat4 transform;
                glm::mat4 localTransform;
                std::string name;
                int index;
                int parentIdx;
            };

            tinygltf::Model model;
            tinygltf::TinyGLTF gltfContext;

            std::vector<uint32_t> indices;
            std::vector<wmdl::Vertex2> verts;
            std::vector<wmdl::VertexSkinningInfo> skinInfo;
            std::vector<wmdl::SubmeshInfo> submeshes;
            bool isModelSkinned;
            robin_hood::unordered_node_map<int, ConvertedGltfNode> convertedNodes;

            ConvertedGltfNode& convertNode(int nodeIdx, ConvertedGltfNode* parent)
            {
                const tinygltf::Node& n = model.nodes[nodeIdx];
                robin_hood::unordered_node_map<int, ConvertedGltfNode>::iterator convertedIterator;

                {
                    ConvertedGltfNode convertedNode;
                    convertedNode.index = nodeIdx;

                    if (parent)
                    {
                        convertedNode.parentIdx = parent->index;
                    }

                    // In glTF, node transforms can be defined as TRS or as a matrix
                    // Either way, convert to a matrix to make it easier everywhere else
                    if (n.matrix.size())
                    {
                        convertedNode.transform = glm::make_mat4x4(n.matrix.data());
                    }
                    else
                    {
                        std::optional<glm::quat> rotation;
                        std::optional<glm::vec3> translation;
                        std::optional<glm::vec3> scale;

                        if (n.rotation.size())
                        {
                            rotation = glm::make_quat(n.rotation.data());
                        }

                        if (n.translation.size())
                        {
                            translation = glm::make_vec3(n.translation.data());
                        }

                        if (n.scale.size())
                        {
                            scale = glm::make_vec3(n.scale.data());
                        }

                        convertedNode.localTransform =
                            (translation ? glm::translate(glm::mat4(1.0f), translation.value()) : glm::mat4(1.0f)) *
                            (rotation ? glm::mat4(rotation.value()) : glm::mat4(1.0f)) *
                            (scale ? glm::scale(glm::mat4(1.0f), scale.value()) : glm::mat4(1.0f));
                    }

                    glm::mat4 parentTransform = parent ? parent->transform : glm::mat4(1.0f);
                    convertedNode.transform = parentTransform * convertedNode.localTransform;
                    convertedNode.name = n.name;
                    convertedNode.parentIdx = parent ? parent->index : -1;
                    auto result = convertedNodes.insert({nodeIdx, convertedNode});
                    assert(result.second);
                    convertedIterator = result.first;
                }

                ConvertedGltfNode& convertedNode = convertedIterator->second;

                if (n.mesh > -1)
                {
                    const tinygltf::Mesh& mesh = model.meshes[n.mesh];
                    for (size_t primIdx = 0; primIdx < mesh.primitives.size(); primIdx++)
                    {
                        const tinygltf::Primitive& prim = mesh.primitives[primIdx];
                        uint32_t indexOffset = verts.size();

                        // Get the buffers for the various attributes
                        const glm::vec3* positions = nullptr;
                        const glm::vec3* normals = nullptr;
                        const glm::vec2* uvs = nullptr;
                        const void* skindices = nullptr;
                        int skindicesType = 0;
                        const glm::vec4* weights = nullptr;

                        positions = getAttributeData<glm::vec3>(model, prim, "POSITION");
                        normals = getAttributeData<glm::vec3>(model, prim, "NORMAL");
                        if (hasAttribute(prim, "TEXCOORD_0"))
                            uvs = getAttributeData<glm::vec2>(model, prim, "TEXCOORD_0");

                        if (hasAttribute(prim, "JOINTS_0"))
                        {
                            const tinygltf::Accessor& accessor =
                                model.accessors[prim.attributes.find("JOINTS_0")->second];
                            skindicesType = accessor.componentType;
                            skindices = getAttributeData<void>(model, prim, "JOINTS_0");
                        }

                        if (hasAttribute(prim, "WEIGHTS_0"))
                            weights = getAttributeData<glm::vec4>(model, prim, "WEIGHTS_0");

                        bool isPrimitiveSkinned = skindices != nullptr && weights != nullptr;
                        isModelSkinned |= isPrimitiveSkinned;

                        // Load indices into vectors
                        std::vector<uint32_t> primIndices;

                        const tinygltf::Accessor& indicesAccessor = model.accessors[prim.indices];
                        const tinygltf::BufferView& bufferView = model.bufferViews[indicesAccessor.bufferView];
                        const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

                        // We always use 32 bit uint indices, but glTF supports a few different types so convert
                        const void* indexBufferPtr = &buffer.data[indicesAccessor.byteOffset + bufferView.byteOffset];
                        switch (indicesAccessor.componentType)
                        {
                        case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
                            const uint32_t* buffer = reinterpret_cast<const uint32_t*>(indexBufferPtr);

                            for (size_t idx = 0; idx < indicesAccessor.count; idx++)
                            {
                                primIndices.push_back(buffer[idx]);
                            }
                        }
                        break;
                        case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
                            const uint16_t* buffer = reinterpret_cast<const uint16_t*>(indexBufferPtr);

                            for (size_t idx = 0; idx < indicesAccessor.count; idx++)
                            {
                                primIndices.push_back(buffer[idx]);
                            }
                        }
                        break;
                        case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
                            const uint8_t* buffer = reinterpret_cast<const uint8_t*>(indexBufferPtr);

                            for (size_t idx = 0; idx < indicesAccessor.count; idx++)
                            {
                                primIndices.push_back(buffer[idx]);
                            }
                        }
                        break;
                        }

                        // De-index the vertices and make them into one big buffer for tangent calculation
                        std::vector<wmdl::Vertex2> primVerts;
                        primVerts.reserve(primIndices.size());

                        for (size_t idx = 0; idx < primIndices.size(); idx++)
                        {
                            size_t v = primIndices[idx];
                            wmdl::Vertex2 vert{};
                            if (isPrimitiveSkinned)
                            {
                                vert.position = positions[v];
                                vert.normal = normals[v];
                            }
                            else
                            {
                                vert.position = convertedNode.transform * glm::vec4(positions[v], 1.0f);
                                vert.normal =
                                    glm::transpose(glm::inverse(convertedNode.transform)) * glm::vec4(normals[v], 1.0f);
                            }
                            vert.uv = uvs ? uvs[v] : glm::vec2(0.0f);
                            primVerts.push_back(vert);
                        }

                        std::vector<wmdl::VertexSkinningInfo> primSkinfos;

                        if (isPrimitiveSkinned)
                        {
                            primSkinfos.reserve(primVerts.size());

                            for (size_t idx = 0; idx < primVerts.size(); idx++)
                            {
                                size_t v = primIndices[idx];
                                wmdl::VertexSkinningInfo skinfo{};
                                for (int i = 0; i < 4; i++)
                                {
                                    if (skindicesType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE)
                                        skinfo.boneId[i] = reinterpret_cast<const uint8_t*>(skindices)[v * 4 + i];
                                    else if (skindicesType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT)
                                        skinfo.boneId[i] = reinterpret_cast<const uint16_t*>(skindices)[v * 4 + i];

                                    skinfo.boneWeight[i] = weights[v][i];
                                }

                                primSkinfos.push_back(skinfo);
                            }
                        }

                        std::vector<wmdl::Vertex2> mikkTSpaceOut(primVerts.size());
                        std::vector<wmdl::VertexSkinningInfo> mikkTSpaceOutSkinInfo(primSkinfos.size());

                        assert(!isPrimitiveSkinned || primVerts.size() == primSkinfos.size());
                        TangentCalcCtx tCalcCtx{primVerts, mikkTSpaceOut, primSkinfos, mikkTSpaceOutSkinInfo};

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
                        primVerts.resize(mikkTSpaceOut.size());

                        // Re-weld the mesh to convert back to indices
                        // Also automatically joins identical vertices
                        int finalVertCount =
                            WeldMesh(remapTable.data(), (float*)primVerts.data(), (float*)mikkTSpaceOut.data(),
                                     mikkTSpaceOut.size(), sizeof(wmdl::Vertex2) / sizeof(float));
                        primVerts.resize(finalVertCount);

                        size_t offsetOfIndices = indices.size();
                        // Copy the actual indices and vertices to the buffer
                        for (int i = 0; i < mikkTSpaceOut.size(); i++)
                        {
                            indices.push_back(remapTable[i] + indexOffset);
                        }

                        // Now copy the skinning info back
                        if (isModelSkinned)
                        {
                            if (isPrimitiveSkinned)
                            {
                                primSkinfos.resize(finalVertCount);
                                for (int i = 0; i < mikkTSpaceOut.size(); i++)
                                {
                                    primSkinfos[remapTable[i]] = mikkTSpaceOutSkinInfo[i];
                                }
                            }
                            else
                            {
                                primSkinfos.clear();
                                primSkinfos.reserve(finalVertCount);
                                for (size_t i = 0; i < finalVertCount; i++)
                                {
                                    primSkinfos.emplace_back();
                                }
                            }
                            skinInfo.insert(skinInfo.end(), primSkinfos.begin(), primSkinfos.end());
                        }

                        verts.insert(verts.end(), primVerts.begin(), primVerts.end());

                        // Add the submesh info
                        wmdl::SubmeshInfo smInfo{};
                        smInfo.numVerts = mikkTSpaceOut.size();
                        smInfo.numIndices = primIndices.size();
                        smInfo.indexOffset = offsetOfIndices;
                        smInfo.materialIndex = prim.material;
                        submeshes.push_back(smInfo);
                    }
                }

                for (int childIndex : n.children)
                {
                    convertNode(childIndex, &convertedNode);
                }

                return convertedNode;
            }

            template <typename T>
            const T* getAttributeData(const tinygltf::Model& model, const tinygltf::Primitive& prim,
                                      const char* attribute)
            {
                const tinygltf::Accessor& accessor = model.accessors[prim.attributes.find(attribute)->second];
                const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                size_t totalOffset = accessor.byteOffset + bufferView.byteOffset;
                return reinterpret_cast<const T*>(&model.buffers[bufferView.buffer].data[totalOffset]);
            }

            size_t getAttributeCount(const tinygltf::Model& model, const tinygltf::Primitive& prim,
                                     const char* attribute)
            {
                const tinygltf::Accessor& accessor = model.accessors[prim.attributes.find(attribute)->second];
                return accessor.count;
            }

            bool hasAttribute(const tinygltf::Primitive& prim, const char* attribute)
            {
                return prim.attributes.contains(attribute);
            }

          public:
            ErrorCodes convertGltfModel(AssetCompileOperation* compileOp, PHYSFS_File* outFile, void* data,
                                        size_t dataSize, ConversionSettings settings)
            {
                // Load the actual model
                std::string errString;
                std::string warnString;
                gltfContext.LoadBinaryFromMemory(&model, &errString, &warnString, (uint8_t*)data, dataSize);
                isModelSkinned = model.skins.size() > 0;
                logMsg("skins: %i", model.skins.size());

                // For now, just assume there's one scene
                const tinygltf::Scene& scene = model.scenes[0];

                for (size_t i = 0; i < scene.nodes.size(); i++)
                {
                    convertNode(scene.nodes[i], nullptr);
                }

                std::vector<wmdl::Bone> bones;

                for (const tinygltf::Skin& skin : model.skins)
                {
                    const tinygltf::Accessor& accessor = model.accessors[skin.inverseBindMatrices];
                    const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                    const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

                    std::vector<glm::mat4> inverseBindMatrices;
                    inverseBindMatrices.resize(accessor.count);
                    memcpy(inverseBindMatrices.data(), &buffer.data[accessor.byteOffset + bufferView.byteOffset],
                           accessor.count * sizeof(glm::mat4));

                    robin_hood::unordered_map<int, int> nodeToJoint;
                    size_t i = 0;
                    for (int jointIdx : skin.joints)
                    {
                        ConvertedGltfNode& jointNode = convertedNodes.at(jointIdx);
                        nodeToJoint.insert({jointNode.index, i});
                        glm::mat4 transform = convertedNodes.at(jointIdx).localTransform;

                        wmdl::Bone b;
                        // For now, just set the parent bone index to the
                        // parent *node* index
                        // We need to do a second pass and fix these later
                        b.parentBone = jointNode.parentIdx;
                        b.transform = transform;
                        b.inverseBindPose = inverseBindMatrices[i];
                        b.setName(jointNode.name.c_str());
                        bones.push_back(b);
                        i++;
                    }

                    for (wmdl::Bone& b : bones)
                    {
                        if (nodeToJoint.contains(b.parentBone))
                            b.parentBone = nodeToJoint[b.parentBone];
                        else
                            b.parentBone = ~0u;
                    }
                }

                size_t vertSkinInfoLength = skinInfo.size() * sizeof(wmdl::VertexSkinningInfo);
                size_t boneLength = bones.size() * sizeof(wmdl::Bone);
                wmdl::Header hdr{};
                hdr.useSmallIndices = verts.size() < UINT16_MAX;

                hdr.numSubmeshes = submeshes.size();
                hdr.submeshOffset = sizeof(hdr) + sizeof(wmdl::SkinningInfoBlock) + vertSkinInfoLength + boneLength;
                hdr.vertexOffset = hdr.submeshOffset + (sizeof(wmdl::SubmeshInfo) * submeshes.size());
                hdr.indexOffset = hdr.vertexOffset + verts.size() * sizeof(wmdl::Vertex2);
                hdr.numVertices = verts.size();
                hdr.numIndices = indices.size();

                PHYSFS_writeBytes(outFile, &hdr, sizeof(hdr));

                wmdl::SkinningInfoBlock sib{};
                sib.numBones = bones.size();
                sib.boneOffset = sizeof(hdr) + sizeof(sib);
                sib.skinningInfoOffset = sib.boneOffset + boneLength;
                PHYSFS_writeBytes(outFile, &sib, sizeof(sib));

                PHYSFS_writeBytes(outFile, bones.data(), boneLength);
                PHYSFS_writeBytes(outFile, skinInfo.data(), vertSkinInfoLength);
                PHYSFS_writeBytes(outFile, submeshes.data(), sizeof(wmdl::SubmeshInfo) * submeshes.size());
                PHYSFS_writeBytes(outFile, verts.data(), sizeof(wmdl::Vertex2) * verts.size());
                if (!hdr.useSmallIndices)
                {
                    PHYSFS_writeBytes(outFile, indices.data(), sizeof(uint32_t) * indices.size());
                }
                else
                {
                    std::vector<uint16_t> smallIndices;
                    smallIndices.resize(indices.size());

                    for (size_t i = 0; i < indices.size(); i++)
                    {
                        smallIndices[i] = (uint16_t)indices[i];
                    }

                    PHYSFS_writeBytes(outFile, smallIndices.data(), sizeof(uint16_t) * smallIndices.size());
                }

                compileOp->progress = 1.0f;
                compileOp->complete = true;
                compileOp->result = CompilationResult::Success;

                return ErrorCodes::None;
            }
        };
    }

    ModelCompiler::ModelCompiler()
    {
        AssetCompilers::registerCompiler(this);
    }

    AssetCompileOperation* ModelCompiler::compile(std::string_view projectRoot, AssetID src)
    {
        std::string inputPath = AssetDB::idToPath(src);
        auto jsonContents = LoadFileToString(inputPath);

        if (jsonContents.error != IOError::None)
        {
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
        AssetCompileOperation* compileOp = new AssetCompileOperation;

        if (result.error != IOError::None)
        {
            logErr("Error opening source path %s", modelSourcePath.c_str());
            compileOp->result = CompilationResult::Error;
            compileOp->complete = true;
            return compileOp;
        }

        compileOp->outputId = AssetDB::pathToId(outputPath);

        std::filesystem::path fullPath = projectRoot;
        fullPath /= outputPath;
        fullPath = fullPath.parent_path();
        fullPath = fullPath.lexically_normal();

        std::filesystem::path fullSourcePath = projectRoot;
        fullSourcePath /= modelSourcePath;
        fullSourcePath = fullSourcePath.lexically_normal();

        std::filesystem::create_directories(fullPath);
        ConversionSettings settings;
        settings.preTransformVerts = j.value("preTransformVerts", false);
        settings.removeRedundantMaterials = j.value("removeRedundantMaterials", true);
        settings.uniformScale = j.value("uniformScale", 1.0f);

        std::thread([compileOp, outputPath, fullSourcePath, path, result, fileLen, settings]() {
            PHYSFS_File* outFile = PHYSFS_openWrite(outputPath.c_str());
            slib::Path p = path;
            if (p.fileExtension() == ".glb")
            {
                mc_internal::GltfModelConverter converter;
                converter.convertGltfModel(compileOp, outFile, result.value, fileLen, settings);
            }
            else if (p.fileExtension() == ".blend")
            {
// If it's a Blender file, we first have to convert to .glb in a temporary directory
#ifdef _WIN32
                slib::String blenderExe = "\"C:/Program Files (x86)/Steam/steamapps/common/Blender/blender.exe\"";
#else
                slib::String blenderExe = "blender";
#endif
                slib::String commandString = blenderExe + " " + slib::String(fullSourcePath.string().c_str()) +
                                             " --background --python blender_glbexport.py -- blender_model_import.glb";
                slib::Subprocess sb{commandString};
                sb.waitForFinish();

                FILE* glbFile = fopen("blender_model_import.glb", "rb");

                if (glbFile)
                {
                    fseek(glbFile, 0, SEEK_END);
                    size_t size = ftell(glbFile);
                    fseek(glbFile, 0, SEEK_SET);
                    char* glbData = new char[size];
                    fread(glbData, 1, size, glbFile);
                    fclose(glbFile);
                    mc_internal::GltfModelConverter converter;
                    converter.convertGltfModel(compileOp, outFile, glbData, size, settings);

                    delete[] glbData;
                    remove("blender_model_import.glb");
                }
                else
                {
                    compileOp->progress = 1.0f;
                    compileOp->complete = 1.0f;
                    compileOp->result = CompilationResult::Error;
                }
            }
            else
            {
                mc_internal::convertAssimpModel(compileOp, outFile, result.value, fileLen, p.fileExtension().cStr(),
                                                settings);
            }
            PHYSFS_close(outFile);
        }).detach();

        return compileOp;
    }

    void ModelCompiler::getFileDependencies(AssetID src, std::vector<std::string>& out)
    {
        std::string inputPath = AssetDB::idToPath(src);
        auto jsonContents = LoadFileToString(inputPath);

        if (jsonContents.error != IOError::None)
        {
            logErr("Error opening asset file");
            return;
        }

        nlohmann::json j = nlohmann::json::parse(jsonContents.value);
        out.push_back(j["srcPath"]);
    }

    const char* ModelCompiler::getSourceExtension()
    {
        return ".wmdlj";
    }

    const char* ModelCompiler::getCompiledExtension()
    {
        return ".wmdl";
    }
}
