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
        isSrgb = j.value("isSrgb", true);
    }

    void TextureEditor::drawEditor() {
        ImGui::Text("Source texture: %s", AssetDB::idToPath(srcTexture).c_str());
        ImGui::SameLine();
        selectAssetPopup("Source Texture", srcTexture, ImGui::Button("Change##SrcTex"));

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

        ImGui::Checkbox("Is SRGB", &isSrgb);
    }

    void TextureEditor::save() {
        nlohmann::json j = {
            { "srcPath", AssetDB::idToPath(srcTexture) },
            { "type", textureTypeSerializedKeys[(int)texType] },
            { "isSrgb", isSrgb }
        };

        std::string s = j.dump(4);
        std::string path = AssetDB::idToPath(editingID);
        FILE* file = fopen(path.c_str(), "wb");
        fwrite(s.data(), 1, s.size(), file);
        fclose(file);
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
