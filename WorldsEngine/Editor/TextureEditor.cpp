#include "TextureEditor.hpp"
#include "../Core/Log.hpp"
#include "../IO/IOUtil.hpp"
#include "Editor/GuiUtil.hpp"
#include "ImGui/imgui.h"
#include <nlohmann/json.hpp>
#include <filesystem>

namespace worlds {
    const char* textureTypeNames[] = {
        "Crunch",
        "RGBA",
        "Packed PBR Map"
    };

    void TextureEditor::importAsset(std::string filePath, std::string newAssetPath) {
        newAssetPath = "SourceData/" + newAssetPath;
        AssetID id = AssetDB::createAsset(newAssetPath);

        PHYSFS_File* f = PHYSFS_openWrite(newAssetPath.c_str());
        nlohmann::json j = {
            { "sourceTexture", filePath },
            { "type", "crunch" },
            { "isSrgb", true },
            { "isNormalMap", false }
        };

        std::string sourceFileName = std::filesystem::path{ filePath }.filename().string();
        // Let's do some file name guessing...
        if (sourceFileName.find("Normal") != std::string::npos) {
            j["isSrgb"] = false;
            j["isNormalMap"] = true;
        }

        if (sourceFileName.find("forcelin") != std::string::npos) {
            j["isSrgb"] = false;
        }

        if (sourceFileName.find("PBRPack") != std::string::npos) {
            j["isSrgb"] = false;
        }

        std::string serializedJson = j.dump(4);
        PHYSFS_writeBytes(f, serializedJson.data(), serializedJson.size());
        PHYSFS_close(f);
        open(id);
    }

    void TextureEditor::create(std::string path) {
        AssetID id = AssetDB::createAsset(path);
        PHYSFS_File* f = PHYSFS_openWrite(path.c_str());
        const char emptyJson[] = "{}";
        PHYSFS_writeBytes(f, emptyJson, sizeof(emptyJson));
        PHYSFS_close(f);
        open(id);
    }

    void TextureEditor::open(AssetID id) {
        editingID = id;

        std::string contents = LoadFileToString(AssetDB::idToPath(id)).value;
        nlohmann::json j = nlohmann::json::parse(contents);
        currentAssetSettings = TextureAssetSettings::fromJson(j);
    }

    void TextureEditor::drawEditor() {
        if (ImGui::BeginCombo("Type", textureTypeNames[(int)currentAssetSettings.type])) {
            int i = 0;
            for (const char* name : textureTypeNames) {
                bool isSelected = (int)currentAssetSettings.type == i;

                if (ImGui::Selectable(name, &isSelected)) {
                    currentAssetSettings.type = static_cast<TextureAssetType>(i);
                    currentAssetSettings.initialiseForType(static_cast<TextureAssetType>(i));
                }

                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }

                i++;
            }
            ImGui::EndCombo();
        }

        if (currentAssetSettings.type == TextureAssetType::Crunch) {
            CrunchTextureSettings& cts = currentAssetSettings.crunch;
            ImGui::Checkbox("Is SRGB", &cts.isSrgb);
            ImGui::Checkbox("Is Normal Map", &cts.isNormalMap);
            ImGui::SliderInt("Quality Level", &cts.qualityLevel, 0, 255);

            if (cts.sourceTexture != INVALID_ASSET)
                ImGui::Text("Source texture: %s", AssetDB::idToPath(cts.sourceTexture).c_str());
            else
                ImGui::Text("Source texture: not set");

            ImGui::SameLine();
            selectRawAssetPopup("Source Texture", cts.sourceTexture, ImGui::Button("Change##SrcTex"));
        } else if (currentAssetSettings.type == TextureAssetType::PBR) {
            PBRTextureSettings& pts = currentAssetSettings.pbr;

            ImGui::DragFloat("Default Metallic", &pts.defaultMetallic);
            ImGui::DragFloat("Default Roughness", &pts.defaultRoughness);
            ImGui::DragFloat("Default Occlusion", &pts.defaultOcclusion);

            if (pts.metallicSource != INVALID_ASSET) {
                ImGui::Text("Metallic texture: %s", AssetDB::idToPath(pts.metallicSource).c_str());
            } else {
                ImGui::Text("No metallic texture set");
            }

            selectRawAssetPopup("Metallic Texture", pts.metallicSource, ImGui::Button("Change Metallic"));

            if (pts.roughnessSource != INVALID_ASSET) {
                ImGui::Text("Roughness texture: %s", AssetDB::idToPath(pts.roughnessSource).c_str());
            } else {
                ImGui::Text("No roughness texture set");
            }

            selectRawAssetPopup("Roughness Texture", pts.roughnessSource, ImGui::Button("Change Roughness"));

            if (pts.occlusionSource != INVALID_ASSET) {
                ImGui::Text("Occlusion texture: %s", AssetDB::idToPath(pts.occlusionSource).c_str());
            } else {
                ImGui::Text("No occlusion texture set");
            }

            selectRawAssetPopup("Occlusion Texture", pts.occlusionSource, ImGui::Button("Change Occlusion"));

            ImGui::SliderInt("Quality Level", &pts.qualityLevel, 0, 255);
        }
    }

    void TextureEditor::save() {
        nlohmann::json j;
        currentAssetSettings.toJson(j);

        std::string s = j.dump(4);
        std::string path = AssetDB::idToPath(editingID);
        PHYSFS_file* file = PHYSFS_openWrite(path.c_str());
        PHYSFS_writeBytes(file, s.data(), s.size());
        PHYSFS_close(file);
    }

    const char* TextureEditor::getHandledExtension() {
        return ".wtexj";
    }
}
