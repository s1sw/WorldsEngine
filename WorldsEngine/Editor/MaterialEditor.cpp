#include "Editor/Editor.hpp"
#include "ImGui/imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include <ImGui/imgui_internal.h>
#include "MaterialEditor.hpp"
#include <Editor/GuiUtil.hpp>
#include <Core/AssetDB.hpp>
#include <Core/Log.hpp>
#include <Util/JsonUtil.hpp>
#include <Render/Render.hpp>
#include "robin_hood.h"
#include <Core/Engine.hpp>
#include <Util/CreateModelObject.hpp>
#include <Util/VKImGUIUtil.hpp>
#include <Util/MathsUtil.hpp>

namespace worlds {
    robin_hood::unordered_map<AssetID, ImTextureID> cacheTextures;
    entt::registry previewRegistry;
    ImTextureID previewPassTex;

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
    std::string getJson(const std::string& key, T val) {
        return "    \"" + key + "\"" + " : " + std::to_string(val);
    }

    std::string getJson(const std::string& key, std::string value) {
        return "    \"" + key + "\"" + " : \"" + value + '"';
    }

    std::string getJson(const std::string& key, glm::vec3 value) {
        return "    \"" + key + "\"" + " : [" +
            std::to_string(value.x) + ", " + std::to_string(value.y) + ", " + std::to_string(value.z) + "]";
    }

    void assetButton(AssetID& id, const char* title, IUITextureManager& texMan) {
        std::string buttonLabel = "Set##";
        buttonLabel += title;

        bool open = false;

        if (id == ~0u || !AssetDB::exists(id))
            open = ImGui::Button(buttonLabel.c_str());
        else {
            if (!cacheTextures.contains(id))
                cacheTextures.insert({ id, texMan.loadOrGet(id) });

            open = ImGui::ImageButton(cacheTextures.at(id), ImVec2(128, 128));
        }

        selectAssetPopup(title, id, open);
    }

    void saveMaterial(EditableMaterial& mat, AssetID matId) {
        std::string j = "{\n";

        j += getJson("albedoPath", AssetDB::idToPath(mat.albedo));

        if (!mat.usePBRMap) {
            if (mat.roughMap != INVALID_ASSET)
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

    void setIfExists(const std::string& path, AssetID& toSet) {
        if (PHYSFS_exists(path.c_str())) {
            toSet = AssetDB::pathToId(path);
        }
    }

    AssetID getAssetId(const nlohmann::json& j, const char* key) {
        if (!j.contains(key))
            return ~0u;
        return AssetDB::pathToId(j[key].get<std::string>());
    }

    void MaterialEditor::setInterfaces(EngineInterfaces interfaces) {
        previewRegistry.set<SceneSettings>(AssetDB::pathToId("Cubemaps/Miramar/miramar.json"), 1.0f);

        RTTPassCreateInfo pci{};
        pci.enableShadows = false;
        pci.msaaLevel = 0;
        pci.cam = &previewCam;
        pci.registryOverride = &previewRegistry;
        pci.width = 256;
        pci.height = 256;
        rttPass = interfaces.renderer->createRTTPass(pci);

        previewEntity = createModelObject(previewRegistry, glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
            AssetDB::pathToId("Models/sphere.wmdl"), AssetDB::pathToId("Materials/DevTextures/dev_metal.json"));

        previewPassTex = (ImTextureID)VKImGUIUtil::createDescriptorSetFor(
            static_cast<VKRTTPass*>(rttPass)->sdrFinalTarget->image(), static_cast<VKRenderer*>(interfaces.renderer)->getHandles());

        previewCam.position = glm::vec3(0.0f, 0.0f, -1.0f);
        previewCam.rotation = glm::angleAxis(0.0f, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::angleAxis(0.0f, glm::vec3(1.0f, 0.0f, 0.0f));

        rttPass->active = true;
        this->interfaces = interfaces;
    }

    bool unloadNext = false;
    EditableMaterial mat;
    bool needsReload = false;

    void MaterialEditor::importAsset(std::string filePath, std::string newAssetPath) {
        logErr("You can't import materials! You will regret this!");
    }

    void MaterialEditor::create(std::string path) {
        AssetID id = AssetDB::createAsset(path);
        PHYSFS_File* f = PHYSFS_openWrite(path.c_str());
        const char emptyJson[] = "{}";
        PHYSFS_writeBytes(f, emptyJson, sizeof(emptyJson));
        PHYSFS_close(f);
        open(id);
    }

    void MaterialEditor::save() {
        saveMaterial(mat, editingID);
    }

    void MaterialEditor::open(AssetID id) {
        editingID = id;
        needsReload = true;
    }

    void MaterialEditor::drawEditor() {
        static bool dragging = false;

        if (unloadNext) {
            for (auto& p : cacheTextures) {
                interfaces.renderer->uiTextureManager().unload(p.first);
            }
            cacheTextures.clear();
            unloadNext = false;
        }

        ImGui::Columns(2);
        if (editingID != ~0u) {
            if (needsReload) {
                auto* f = AssetDB::openAssetFileRead(editingID);
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

            previewRegistry.get<WorldObject>(previewEntity).materials[0] = editingID;

            ImGui::SliderFloat("Metallic", &mat.metallic, 0.0f, 1.0f);
            ImGui::SliderFloat("Roughness", &mat.roughness, 0.0f, 1.0f);

            if (mat.heightMap != ~0u)
                ImGui::SliderFloat("Heightmap Scale", &mat.heightmapScale, 0.0f, 0.1f);

            ImGui::ColorEdit3("Albedo Color", &mat.albedoColor.x);
            ImGui::ColorEdit3("Emissive Color", &mat.emissiveColor.x, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);

            auto& texMan = interfaces.renderer->uiTextureManager();
            if (mat.albedo != ~0u) {
                ImGui::Text("Current albedo path: %s", AssetDB::idToPath(mat.albedo).c_str());
            } else {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "Invalid albedo map");
            }
            assetButton(mat.albedo, "Albedo", texMan);

            if (mat.normalMap != ~0u) {
                ImGui::Text("Current normal map path: %s", AssetDB::idToPath(mat.normalMap).c_str());
            } else {
                ImGui::Text("No normal map set");
            }
            assetButton(mat.normalMap, "Normal map", texMan);

            if (mat.heightMap != ~0u) {
                ImGui::Text("Current height map path: %s", AssetDB::idToPath(mat.heightMap).c_str());
            } else {
                ImGui::Text("No height map set");
            }
            assetButton(mat.heightMap, "Height map", texMan);

            ImGui::Checkbox("Use packed PBR map", &mat.usePBRMap);
            if (mat.usePBRMap) {
                if (mat.pbrMap != ~0u) {
                    ImGui::Text("Current PBR map path: %s", AssetDB::idToPath(mat.pbrMap).c_str());
                } else {
                    ImGui::Text("No PBR map set");
                }

                assetButton(mat.pbrMap, "PBR Map", texMan);
            } else {
                if (mat.metalMap != ~0u) {
                    ImGui::Text("Current metallic map path: %s", AssetDB::idToPath(mat.metalMap).c_str());
                } else {
                    ImGui::Text("No metallic map set");
                }

                assetButton(mat.metalMap, "Metal Map", texMan);

                if (mat.roughMap != ~0u) {
                    ImGui::Text("Current roughness map path: %s", AssetDB::idToPath(mat.roughMap).c_str());
                } else {
                    ImGui::Text("No roughness map set");
                }

                assetButton(mat.roughMap, "Rough Map", texMan);
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

            ImGui::NextColumn();

            static ImVec2 lastPreviewSize{ 0.0f, 0.0f };
            ImVec2 previewSize = ImGui::GetContentRegionAvail() - ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 2.0f);
            bool resized = false;
            if (previewSize.x != lastPreviewSize.x || previewSize.y != lastPreviewSize.y) {
                rttPass->resize(std::max(16, (int)previewSize.x), std::max(16, (int)previewSize.y));
                VKImGUIUtil::updateDescriptorSet((VkDescriptorSet)previewPassTex, static_cast<VKRTTPass*>(rttPass)->sdrFinalTarget->image());
                lastPreviewSize = previewSize;
                resized = true;
                rttPass->active = true;
            }

            ImVec2 cpos = ImGui::GetWindowPos() + ImGui::GetCursorPos() - ImVec2(ImGui::GetScrollX(), ImGui::GetScrollY());
            ImVec2 end = previewSize + cpos;
            ImGui::ImageButton(previewPassTex, previewSize, ImVec2(0, 0), ImVec2(1, 1), 0);

            static float lx = 0.0f;
            static float ly = 0.0f;
            static float dist = 2.0f;

            if (ImGui::IsMouseHoveringRect(cpos, end)) {
                rttPass->active = true;
                if (ImGui::IsMouseDown(0)) {
                    dragging = true;
                }

                dist -= ImGui::GetIO().MouseWheel * 0.25;
                dist = glm::max(dist, 0.25f);
            } else if (!resized) {
                rttPass->active = false;
            }

            if (!ImGui::IsMouseDown(0)) {
                dragging = false;
            }

            if (dragging) {
                lx -= ImGui::GetIO().MouseDelta.x * 0.01f;
                ly -= ImGui::GetIO().MouseDelta.y * 0.01f;

                ly = glm::clamp(ly, -glm::half_pi<float>() + 0.1f, glm::half_pi<float>() - 0.1f);
            }

            glm::quat q = glm::angleAxis(lx, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::angleAxis(ly, glm::vec3(1.0f, 0.0f, 0.0f));

            glm::vec3 dir = q * glm::vec3(0.0f, 0.0f, 1.0f);
            previewCam.position = dir * dist;
            previewCam.rotation = glm::quatLookAt(dir, glm::vec3(0.0f, 1.0f, 0.0f));

            if (ImGui::Button("Box")) {
                previewRegistry.get<WorldObject>(previewEntity).mesh = AssetDB::pathToId("Models/cube.wmdl");
            }

            ImGui::SameLine();
            if (ImGui::Button("Sphere")) {
                previewRegistry.get<WorldObject>(previewEntity).mesh = AssetDB::pathToId("Models/sphere.wmdl");
            }

            ImGui::SameLine();
            if (ImGui::Button("Plane")) {
                previewRegistry.get<WorldObject>(previewEntity).mesh = AssetDB::pathToId("Models/plane.wmdl");
            }

            static AssetID customModel = ~0u;
            ImGui::SameLine();
            bool openModelPopup = false;
            if (ImGui::Button("Other Model")) {
                openModelPopup = true;
            }

            ImGui::SameLine();

            bool openSkyboxPopup = false;
            auto& sceneSettings = previewRegistry.ctx<SceneSettings>();

            if (ImGui::Button("Change Background")) {
                openSkyboxPopup = true;
            }

            if (selectAssetPopup("Preview Model", customModel, openModelPopup)) {
                previewRegistry.get<WorldObject>(previewEntity).mesh = customModel;
            }

            selectAssetPopup("Skybox", sceneSettings.skybox, openSkyboxPopup);
        } else {
            bool open = ImGui::Button("Open Material");
            needsReload = selectAssetPopup("Select Material", editingID, open, true);
            rttPass->active = false;
        }

        ImGui::EndColumns();

        //if (!active && cacheTextures.size() > 0) {
        //    unloadNext = true;
        //}
    }

    const char* MaterialEditor::getHandledExtension() {
        return ".json";
    }
}
