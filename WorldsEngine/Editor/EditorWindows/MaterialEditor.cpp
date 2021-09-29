#include "../../ImGui/imgui.h"
#include "EditorWindows.hpp"
#include "../GuiUtil.hpp"
#include "../../Core/AssetDB.hpp"
#include "../../Core/Log.hpp"
#include "../../Util/JsonUtil.hpp"

namespace worlds {
    robin_hood::unordered_map<AssetID, ImTextureID> cacheTextures;
    struct EditableMaterial {
        AssetID albedo = ~0u;
        AssetID normalMap = ~0u;
        AssetID metalMap = ~0u;
        AssetID roughMap = ~0u;
        AssetID heightMap = ~0u;
        AssetID pbrMap = ~0u;
        glm::vec3 albedoColor = glm::vec3{ 1.0f, 1.0f, 1.0f };
        glm::vec3 emissiveColor = glm::vec3{ 0.0f, 0.0f, 0.0f };
        float metallic = 0.0f;
        float roughness = 0.75f;
        float alphaCutoff = 0.0f;
        float heightmapScale = 0.0f;
        bool cullOff = false;
        bool wireframe = false;
        bool usePBRMap = false;
        bool useAlphaTest = false;
    };

    template <typename T>
    std::string getJson(std::string key, T val) {
        return "    \"" + key + "\"" + " : " + std::to_string(val);
    }

    std::string getJson(std::string key, std::string value) {
        return "    \"" + key + "\"" + " : \"" + value + '"';
    }

    std::string getJson(std::string key, glm::vec3 value) {
        return "    \"" + key + "\"" + " : [" +
            std::to_string(value.x) + ", " + std::to_string(value.y) + ", " + std::to_string(value.z) + "]";
    }

    void assetButton(AssetID& id, const char* title, UITextureManager* texMan) {
        std::string buttonLabel = "Set##";
        buttonLabel += title;

        bool open = false;

        if (id == ~0u || !AssetDB::exists(id))
            open = ImGui::Button(buttonLabel.c_str());
        else {
            if (!cacheTextures.contains(id))
                cacheTextures.insert({ id, texMan->loadOrGet(id) });

            open = ImGui::ImageButton(cacheTextures.at(id), ImVec2(128, 128));
        }

        selectAssetPopup(title, id, open);
    }

    void saveMaterial(EditableMaterial& mat, AssetID matId) {
        std::string j = "{\n";

        j += getJson("albedoPath", AssetDB::idToPath(mat.albedo));

        if (!mat.usePBRMap) {
            if (mat.roughMap != ~0u)
                j += ",\n" + getJson("roughMapPath", AssetDB::idToPath(mat.roughMap));

            if (mat.metalMap != ~0u)
                j += ",\n" + getJson("metalMapPath", AssetDB::idToPath(mat.metalMap));
        } else {
            if (mat.pbrMap != ~0u)
                j += ",\n" + getJson("pbrMapPath", AssetDB::idToPath(mat.pbrMap));
        }

        if (mat.normalMap != ~0u) {
            j += ",\n" + getJson("normalMapPath", AssetDB::idToPath(mat.normalMap));
        }

        if (mat.heightMap != ~0u) {
            j += ",\n" + getJson("heightmapPath", AssetDB::idToPath(mat.heightMap));
            j += ",\n" + getJson("heightmapScale", mat.heightmapScale);
        }

        j += ",\n" + getJson("metallic", mat.metallic);
        j += ",\n" + getJson("roughness", mat.roughness);
        j += ",\n" + getJson("albedoColor", mat.albedoColor);

        if (glm::any(glm::greaterThan(mat.emissiveColor, glm::vec3(0.0f)))) {
            j += ",\n" + getJson("emissiveColor", mat.emissiveColor);
        }

        if (mat.alphaCutoff != 0.0f)
            j += ",\n" + getJson("alphaCutoff", mat.useAlphaTest ? mat.alphaCutoff : 0.0f);

        if (mat.cullOff)
            j += ",\n" + getJson("cullOff", 1);

        if (mat.wireframe)
            j += ",\n" + getJson("wireframe", 1);

        j += "\n}\n";

        logMsg("%s", j.c_str());

        PHYSFS_File* file = AssetDB::openAssetFileWrite(matId);
        if (file != nullptr) {
            PHYSFS_writeBytes(file, j.data(), j.size());
            PHYSFS_close(file);
        } else {
            addNotification("Failed to save material", NotificationType::Error);
        }
    }

    void setIfExists(std::string path, AssetID& toSet) {
        if (PHYSFS_exists(path.c_str())) {
            toSet = AssetDB::pathToId(path);
        }
    }

    AssetID getAssetId(const nlohmann::json& j, const char* key) {
        if (!j.contains(key))
            return ~0u;
        return AssetDB::pathToId(j[key].get<std::string>());
    }

    void MaterialEditor::draw(entt::registry&) {
        static AssetID materialId = ~0u;
        static bool needsReload = false;
        static EditableMaterial mat;

        if (ImGui::Begin("Material Editor", &active)) {
            if (materialId != ~0u) {
                if (needsReload) {
                    auto* f = AssetDB::openAssetFileRead(materialId);
                    size_t fileSize = PHYSFS_fileLength(f);
                    std::string str;
                    str.resize(fileSize);

                    PHYSFS_readBytes(f, str.data(), fileSize);
                    PHYSFS_close(f);

                    nlohmann::json j = nlohmann::json::parse(str);

                    mat.metallic = j.value("metallic", 0.0f);
                    mat.roughness = j.value("roughness", 0.5f);
                    mat.heightmapScale = j.value("heightmapScale", 0.05f);
                    mat.alphaCutoff = j.value("alphaCutoff", 0.5f);
                    mat.albedoColor = j.value("albedoColor", glm::vec3{1.0f});
                    mat.emissiveColor = j.value("emissiveColor", glm::vec3{0.0f});
                    mat.albedo = getAssetId(j, "albedoPath");
                    mat.heightMap = getAssetId(j, "heightmapPath");
                    mat.metalMap = getAssetId(j, "metalMapPath");
                    mat.roughMap = getAssetId(j, "roughMapPath");
                    mat.normalMap = getAssetId(j, "normalMapPath");

                    mat.cullOff = j.contains("cullOff");
                    mat.wireframe = j.contains("wireframe");
                    mat.usePBRMap = j.contains( "pbrMapPath");
                    mat.useAlphaTest = mat.alphaCutoff > 0.0f;

                    if (mat.usePBRMap) {
                        mat.pbrMap = getAssetId(j, "pbrMapPath");
                    }

                    needsReload = false;
                }

                ImGui::SliderFloat("Metallic", &mat.metallic, 0.0f, 1.0f);
                ImGui::SliderFloat("Roughness", &mat.roughness, 0.0f, 1.0f);

                if (mat.heightMap != ~0u)
                    ImGui::SliderFloat("Heightmap Scale", &mat.heightmapScale, 0.0f, 0.1f);

                ImGui::ColorEdit3("Albedo Color", &mat.albedoColor.x);
                ImGui::ColorEdit3("Emissive Color", &mat.emissiveColor.x);

                ImGui::Text("Current albedo path: %s", AssetDB::idToPath(mat.albedo).c_str());
                assetButton(mat.albedo, "Albedo", editor->texManager());

                if (mat.normalMap != ~0u) {
                    ImGui::Text("Current normal map path: %s", AssetDB::idToPath(mat.normalMap).c_str());
                } else {
                    ImGui::Text("No normal map set");
                }
                assetButton(mat.normalMap, "Normal map", editor->texManager());

                if (mat.heightMap != ~0u) {
                    ImGui::Text("Current height map path: %s", AssetDB::idToPath(mat.heightMap).c_str());
                } else {
                    ImGui::Text("No height map set");
                }
                assetButton(mat.heightMap, "Height map", editor->texManager());

                ImGui::Checkbox("Use packed PBR map", &mat.usePBRMap);
                if (mat.usePBRMap) {
                    if (mat.pbrMap != ~0u) {
                        ImGui::Text("Current PBR map path: %s", AssetDB::idToPath(mat.pbrMap).c_str());
                    } else {
                        ImGui::Text("No PBR map set");
                    }

                    assetButton(mat.pbrMap, "PBR Map", editor->texManager());
                } else {
                    if (mat.metalMap != ~0u) {
                        ImGui::Text("Current metallic map path: %s", AssetDB::idToPath(mat.metalMap).c_str());
                    } else {
                        ImGui::Text("No metallic map set");
                    }

                    assetButton(mat.metalMap, "Metal Map", editor->texManager());

                    if (mat.roughMap != ~0u) {
                        ImGui::Text("Current roughness map path: %s", AssetDB::idToPath(mat.roughMap).c_str());
                    } else {
                        ImGui::Text("No roughness map set");
                    }

                    assetButton(mat.roughMap, "Rough Map", editor->texManager());
                }

                ImGui::Checkbox("Alpha Test", &mat.useAlphaTest);

                if (mat.useAlphaTest) {
                    ImGui::DragFloat("Alpha Cutoff", &mat.alphaCutoff);
                }

                if (ImGui::BeginPopup("Open Material from folder + mat name")) {
                    static std::string folderPath = "Materials/";
                    static std::string matName = "";

                    ImGui::InputText("Folder", &folderPath);
                    ImGui::InputText("Material Name", &matName);

                    if (ImGui::Button("Set")) {
                        std::string matPath = folderPath + matName;
                        setIfExists(matPath + "_BaseColor.png", mat.albedo);
                        setIfExists(matPath + "_Normal_forcelin.png", mat.normalMap);
                        setIfExists(matPath + "_PBRPack.png", mat.pbrMap);

                        setIfExists(matPath + "_BaseColor.wtex", mat.albedo);
                        setIfExists(matPath + "_Normal_forcelin.wtex", mat.normalMap);
                        setIfExists(matPath + "_PBRPack.wtex", mat.pbrMap);
                        mat.usePBRMap = true;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }

                if (ImGui::Button("Set from folder + mat name")) {
                    ImGui::OpenPopup("Open Material from folder + mat name");
                }

                if (ImGui::Button("Save")) {
                    saveMaterial(mat, materialId);
                }

                ImGui::SameLine();

                if (ImGui::Button("Close")) {
                    materialId = ~0u;
                }
            } else {
                bool open = ImGui::Button("Open Material");
                needsReload = selectAssetPopup("Select Material", materialId, open);
            }
        }

        ImGui::End();
    }
}
