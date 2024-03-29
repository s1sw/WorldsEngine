#include "ModelEditor.hpp"
#include "Editor/AssetEditors.hpp"
#include <AssetCompilation/AssetCompilerUtil.hpp>
#include <Core/MeshManager.hpp>
#include <Editor/GuiUtil.hpp>
#include <IO/IOUtil.hpp>
#include <ImGui/imgui.h>
#include <nlohmann/json.hpp>

namespace worlds
{
    ModelEditor::ModelEditor(AssetID id)
    {
        editingID = id;

        std::string contents = LoadFileToString(AssetDB::idToPath(id)).value;
        try
        {
            nlohmann::json j = nlohmann::json::parse(contents);
            srcModel = AssetDB::pathToId(j.value("srcPath", "Raw/Models/cube.obj"));
            preTransformVerts = j.value("preTransformVerts", false);
            uniformScale = j.value("uniformScale", 1.0f);
            removeRedundantMaterials = j.value("removeRedundantMaterials", true);
            combineSubmeshes = j.value("combineSubmeshes", false);
        }
        catch (nlohmann::detail::exception& except)
        {
            addNotification(("Error opening " + AssetDB::idToPath(id)), NotificationType::Error);
            srcModel = INVALID_ASSET;
        }
    }

    void ModelEditor::draw()
    {
        if (srcModel != INVALID_ASSET)
            ImGui::Text("Source model: %s", AssetDB::idToPath(srcModel).c_str());
        else
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Invalid source");
        ImGui::SameLine();
        selectRawAssetPopup("Source Model", srcModel, ImGui::Button("Change##SrcModel"));
        ImGui::Checkbox("Pre-Transform Vertices", &preTransformVerts);
        ImGui::Checkbox("Remove redundant materials", &removeRedundantMaterials);
        ImGui::Checkbox("Combine submeshes", &combineSubmeshes);
        tooltipHover("Combines all submeshes with the same material into a single submesh."
                     "Optimises complex models, but means you lose some control over what parts"
                     " of the model can be shown.");
        ImGui::DragFloat("Uniform Scaling", &uniformScale);

        if (AssetDB::exists(srcModel))
        {
            AssetID outputAsset = getOutputAsset(AssetDB::idToPath(editingID));
            ImGui::Text("Compiled path: %s", AssetDB::idToPath(outputAsset).c_str());

            if (AssetDB::exists(outputAsset))
            {
                if (ImGui::Button("Refresh"))
                {
                    MeshManager::unload(outputAsset);
                }

                const LoadedMesh& lm = MeshManager::loadOrGet(outputAsset);
                ImGui::Text("%i submeshes", lm.numSubmeshes);

                glm::vec3 aabbCenter = (lm.aabbMin + lm.aabbMax) * 0.5f;
                ImGui::Text("AABB min: %.3f, %.3f, %.3f", lm.aabbMin.x, lm.aabbMin.y, lm.aabbMin.z);
                ImGui::Text("AABB max: %.3f, %.3f, %.3f", lm.aabbMax.x, lm.aabbMax.y, lm.aabbMax.z);
                ImGui::Text("AABB center: %.3f, %.3f, %.3f", aabbCenter.x, aabbCenter.y, aabbCenter.z);

                if (ImGui::TreeNode("Submeshes"))
                {
                    for (int i = 0; i < lm.numSubmeshes; i++)
                    {
                        ImGui::Text(
                            "%i indices, material %i", lm.submeshes[i].indexCount, lm.submeshes[i].materialIndex);
                    }
                    ImGui::TreePop();
                }

                if (ImGui::TreeNode("Bones"))
                {
                    for (const Bone& v : lm.bones)
                    {
                        ImGui::Text("%s", v.name.cStr());
                    }
                }
            }
        }
    }

    void ModelEditor::save()
    {
        nlohmann::json j = {
            {"srcPath", AssetDB::idToPath(srcModel)},
            {"uniformScale", uniformScale},
            {"removeRedundantMaterials", removeRedundantMaterials}};

        if (preTransformVerts)
            j["preTransformVerts"] = true;

        if (combineSubmeshes)
            j["combineSubmeshes"] = true;

        std::string s = j.dump(4);
        std::string path = AssetDB::idToPath(editingID);
        PHYSFS_File* file = PHYSFS_openWrite(path.c_str());
        PHYSFS_writeBytes(file, s.data(), s.size());
        PHYSFS_close(file);
    }

    bool ModelEditor::hasUnsavedChanges()
    {
        return unsavedChanges;
    }

    void ModelEditorMeta::importAsset(std::string filePath, std::string newAssetPath)
    {
        AssetID id = AssetDB::createAsset(newAssetPath);
        PHYSFS_File* f = PHYSFS_openWrite(("SourceData/" + newAssetPath).c_str());
        nlohmann::json j = {{"srcPath", filePath}};
        std::string serializedJson = j.dump(4);
        PHYSFS_writeBytes(f, serializedJson.data(), serializedJson.size());
        PHYSFS_close(f);
    }

    void ModelEditorMeta::create(std::string path)
    {
        AssetID id = AssetDB::createAsset(path);
        PHYSFS_File* f = PHYSFS_openWrite(path.c_str());
        const char emptyJson[] = "{}";
        PHYSFS_writeBytes(f, emptyJson, sizeof(emptyJson));
        PHYSFS_close(f);
    }

    IAssetEditor* ModelEditorMeta::createEditorFor(AssetID id)
    {
        return new ModelEditor(id);
    }

    const char* ModelEditorMeta::getHandledExtension()
    {
        return ".wmdlj";
    }
}
