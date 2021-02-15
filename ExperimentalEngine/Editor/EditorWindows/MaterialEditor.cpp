#include "../../ImGui/imgui.h"
#include "EditorWindows.hpp"
#include "../GuiUtil.hpp"
#include <sajson.h>
#include "../../Core/AssetDB.hpp"
#include "../../Core/Log.hpp"
#include "../../Util/JsonUtil.hpp"

namespace worlds {
    struct EditableMaterial {
        AssetID albedo = ~0u;
        AssetID normalMap = ~0u;
        AssetID metalMap = ~0u;
        AssetID roughMap = ~0u;
        AssetID heightMap = ~0u;
        glm::vec3 albedoColor = glm::vec3{ 1.0f, 1.0f, 1.0f };
        glm::vec3 emissiveColor = glm::vec3{ 0.0f, 0.0f, 0.0f };
        float metallic = 0.0f;
        float roughness = 0.75f;
        float alphaCutoff = 0.0f;
        float heightmapScale = 0.0f;
        bool cullOff = false;
        bool wireframe = false;
    };

    void getAssetId(const sajson::value& obj, const char* key, AssetID& aid) {
        sajson::string keyStr{ key, strlen(key) };

        auto idx = obj.find_object_key(keyStr);

        if (idx == obj.get_length())
            return;

        auto path = obj.get_object_value(idx).as_string();

        aid = g_assetDB.addOrGetExisting(path);
    }

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

    void MaterialEditor::draw(entt::registry& reg) {
        static AssetID materialId = ~0u;
        static bool needsReload = false;
        static EditableMaterial mat;

        if (ImGui::Begin("Material Editor", &active)) {
            if (materialId != ~0u) {
                if (needsReload) {
                    PHYSFS_File* f = g_assetDB.openAssetFileRead(materialId);
                    size_t fileSize = PHYSFS_fileLength(f);
                    char* buffer = (char*)std::malloc(fileSize);
                    PHYSFS_readBytes(f, buffer, fileSize);
                    PHYSFS_close(f);

                    const sajson::document& document = sajson::parse(
                        sajson::single_allocation(), sajson::mutable_string_view(fileSize, buffer)
                    );

                    const auto& root = document.get_root();

                    getFloat(root, "metallic", mat.metallic);
                    getFloat(root, "roughness", mat.roughness);
                    getFloat(root, "heightmapScale", mat.heightmapScale);
                    getFloat(root, "alphaCutoff", mat.alphaCutoff);
                    getVec3(root, "albedoColor", mat.albedoColor);
                    getVec3(root, "emissiveColor", mat.emissiveColor);
                    getAssetId(root, "albedoPath", mat.albedo);
                    getAssetId(root, "heightmapPath", mat.heightMap);
                    getAssetId(root, "metalMapPath", mat.metalMap);
                    getAssetId(root, "roughMapPath", mat.roughMap);
                    getAssetId(root, "normalMapPath", mat.normalMap);

                    mat.cullOff = hasKey(root, "cullOff");
                    mat.wireframe = hasKey(root, "wireframe");

                    std::free(buffer);
                }

                ImGui::SliderFloat("Metallic", &mat.metallic, 0.0f, 1.0f);
                ImGui::SliderFloat("Roughness", &mat.roughness, 0.0f, 1.0f);

                if (mat.heightMap != ~0u)
                    ImGui::SliderFloat("Heightmap Scale", &mat.heightmapScale, 0.0f, 0.1f);

                ImGui::ColorEdit3("Albedo Color", &mat.albedoColor.x);
                ImGui::ColorEdit3("Emissive Color", &mat.emissiveColor.x);

                if (ImGui::Button("Save")) {
                    std::string j = "{\n";

                    j += getJson("albedoPath", g_assetDB.getAssetPath(mat.albedo));

                    if (mat.roughMap != ~0u)
                        j += ",\n" + getJson("roughMapPath", g_assetDB.getAssetPath(mat.roughMap));

                    if (mat.metalMap != ~0u)
                        j += ",\n" + getJson("metalMapPath", g_assetDB.getAssetPath(mat.metalMap));

                    if (mat.heightMap != ~0u) {
                        j += ",\n" + getJson("heightmapPath", g_assetDB.getAssetPath(mat.heightMap));
                        j += ",\n" + getJson("heightmapScale", mat.heightmapScale);
                    }

                    j += ",\n" + getJson("metallic", mat.metallic);
                    j += ",\n" + getJson("roughness", mat.roughness);
                    j += ",\n" + getJson("albedoColor", mat.albedoColor);

                    if (glm::any(glm::greaterThan(mat.emissiveColor, glm::vec3(0.0f)))) {
                        j += ",\n" + getJson("emissiveColor", mat.emissiveColor);
                    }

                    if (mat.alphaCutoff != 0.0f)
                        j += ",\n" + getJson("alphaCutoff", mat.alphaCutoff);

                    if (mat.cullOff)
                        j += ",\n" + getJson("cullOff", 1);

                    if (mat.wireframe)
                        j += ",\n" + getJson("wireframe", 1);


                    j += "\n}\n";

                    logMsg("%s", j.c_str());
                }
            } else {
                bool open = ImGui::Button("Open Material");
                needsReload = selectAssetPopup("Select Material", materialId, open);
            }
        }

        ImGui::End();
    }
}
