#include "TextureEditor.hpp"
#include "../Core/Log.hpp"
#include "../IO/IOUtil.hpp"
#include "Editor/GuiUtil.hpp"
#include "ImGui/imgui.h"
#include <nlohmann/json.hpp>

namespace worlds {
    const char* textureTypeNames[] = {
        "Regular",
        "Normal Map"
    };

    const char* textureTypeSerializedKeys[] = {
        "regular",
        "normal"
    };

    void TextureEditor::create(std::string path) {
        open(AssetDB::createAsset(path));
    }

    void TextureEditor::open(AssetID id) {
        editingID = id;

        std::string contents = LoadFileToString(AssetDB::idToPath(id)).value;
        nlohmann::json j = nlohmann::json::parse(contents);
        srcTexture = AssetDB::pathToId(j.value("srcPath", "SrcData/Raw/Textures/512SimpleOrange.png"));
        texType = strToTexType(j.value("type", "regular"));
    }

    void TextureEditor::drawEditor() {
        if (ImGui::BeginCombo("Type", textureTypeNames[(int)texType])) {
            int i = 0;
            for (const char* name : textureTypeNames) {
                bool isSelected = (int)texType == i;

                if (ImGui::Selectable(name, &isSelected)) {
                    texType = (TextureType)i;
                }

                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }

                i++;
            }
            ImGui::EndCombo();
        }

        ImGui::Text("Source texture: %s", AssetDB::idToPath(srcTexture).c_str());
        ImGui::SameLine();
        selectAssetPopup("Source Texture", srcTexture, ImGui::Button("Change##SrcTex"));
    }

    void TextureEditor::save() {
        nlohmann::json j = {
            { "srcPath", AssetDB::idToPath(srcTexture) },
            { "type", textureTypeSerializedKeys[(int)texType] }
        };

        std::string s = j.dump(4);
        PHYSFS_File* file = AssetDB::openAssetFileWrite(editingID);
        PHYSFS_writeBytes(file, s.data(), s.size());
        PHYSFS_close(file);
    }

    const char* TextureEditor::getHandledExtension() {
        return ".wtexj";
    }

    TextureEditor::TextureType TextureEditor::strToTexType(std::string_view texType) { if (texType == "normal") {
            return TextureType::NormalMap;
        } else if(texType == "regular") {
            return TextureType::Regular;
        } else {
            logErr("Invalid texture type: %s", texType.data());
            return TextureType::Regular;
        }
    }
}
