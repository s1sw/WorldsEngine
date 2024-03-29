#include "Core/IGameEventHandler.hpp"
#include "Editor/AssetEditors.hpp"
#include "Editor/Editor.hpp"
#include "ImGui/imgui.h"
#include "Render/RenderInternal.hpp"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "MaterialEditor.hpp"
#include "robin_hood.h"
#include <Core/AssetDB.hpp>
#include <Core/Engine.hpp>
#include <Core/Log.hpp>
#include <Editor/GuiUtil.hpp>
#include <ImGui/imgui_internal.h>
#include <Render/Render.hpp>
#include <Util/CreateModelObject.hpp>
#include <Util/JsonUtil.hpp>
#include <Util/MathsUtil.hpp>

namespace worlds
{
    robin_hood::unordered_map<AssetID, ImTextureID> cacheTextures;

    template <typename T> std::string getJson(const std::string& key, T val)
    {
        return "    \"" + key + "\"" + " : " + std::to_string(val);
    }

    std::string getJson(const std::string& key, std::string value)
    {
        return "    \"" + key + "\"" + " : \"" + value + '"';
    }

    std::string getJson(const std::string& key, glm::vec3 value)
    {
        return "    \"" + key + "\"" + " : [" + std::to_string(value.x) + ", " + std::to_string(value.y) + ", " +
               std::to_string(value.z) + "]";
    }

    bool assetButton(AssetID& id, const char* title)
    {
        std::string buttonLabel = "Set##";
        buttonLabel += title;

        bool open = false;

        // if (id == ~0u || !AssetDB::exists(id))
        open = ImGui::Button(buttonLabel.c_str());
        // else {
        //    if (!cacheTextures.contains(id))
        //        cacheTextures.insert({ id, texMan.loadOrGet(id) });

        //    open = ImGui::ImageButton(cacheTextures.at(id), ImVec2(128, 128));
        //}

        return selectAssetPopup(title, id, open);
    }

    void saveMaterial(EditableMaterial& mat, AssetID matId)
    {
        std::string j = "{\n";

        j += getJson("albedoPath", AssetDB::idToPath(mat.albedo));

        if (!mat.usePBRMap)
        {
            if (mat.roughMap != INVALID_ASSET)
                j += ",\n" + getJson("roughMapPath", AssetDB::idToPath(mat.roughMap));

            if (mat.metalMap != ~0u)
                j += ",\n" + getJson("metalMapPath", AssetDB::idToPath(mat.metalMap));
        }
        else
        {
            if (mat.pbrMap != ~0u)
                j += ",\n" + getJson("pbrMapPath", AssetDB::idToPath(mat.pbrMap));
        }

        if (mat.normalMap != ~0u)
        {
            j += ",\n" + getJson("normalMapPath", AssetDB::idToPath(mat.normalMap));
        }

        if (mat.heightMap != ~0u)
        {
            j += ",\n" + getJson("heightmapPath", AssetDB::idToPath(mat.heightMap));
            j += ",\n" + getJson("heightmapScale", mat.heightmapScale);
        }

        j += ",\n" + getJson("metallic", mat.metallic);
        j += ",\n" + getJson("roughness", mat.roughness);
        j += ",\n" + getJson("albedoColor", mat.albedoColor);

        if (glm::any(glm::greaterThan(mat.emissiveColor, glm::vec3(0.0f))))
        {
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
        if (file != nullptr)
        {
            PHYSFS_writeBytes(file, j.data(), j.size());
            PHYSFS_close(file);
        }
        else
        {
            addNotification("Failed to save material", NotificationType::Error);
        }
    }

    void setIfExists(const std::string& path, AssetID& toSet)
    {
        if (PHYSFS_exists(path.c_str()))
        {
            toSet = AssetDB::pathToId(path);
        }
    }

    AssetID getAssetId(const nlohmann::json& j, const char* key)
    {
        if (!j.contains(key))
            return ~0u;
        return AssetDB::pathToId(j[key].get<std::string>());
    }

    MaterialEditor::MaterialEditor(AssetID id, EngineInterfaces interfaces) : lx(0.0f), ly(0.0f), dist(2.0f)
    {
        previewRegistry.set<SkySettings>(AssetDB::pathToId("Cubemaps/Miramar/miramar.json"), 1.0f);

        RTTPassSettings pci{};
        pci.enableShadows = false;
        pci.msaaLevel = 0;
        pci.cam = &previewCam;
        pci.registryOverride = &previewRegistry;
        pci.width = 256;
        pci.height = 256;
        pci.renderDebugShapes = false;
        rttPass = interfaces.renderer->createRTTPass(pci);

        previewEntity = createModelObject(previewRegistry, glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                                          AssetDB::pathToId("Models/sphere.wmdl"),
                                          AssetDB::pathToId("Materials/DevTextures/dev_metal.json"));

        previewCam.position = glm::vec3(0.0f, 0.0f, -1.0f);
        previewCam.rotation =
            glm::angleAxis(0.0f, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::angleAxis(0.0f, glm::vec3(1.0f, 0.0f, 0.0f));

        rttPass->active = true;
        this->interfaces = interfaces;
        editingID = id;

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
        mat.alphaCutoff = j.value("alphaCutoff", 0.0f);
        mat.albedoColor = j.value("albedoColor", glm::vec3{1.0f});
        mat.emissiveColor = j.value("emissiveColor", glm::vec3{0.0f});
        mat.albedo = getAssetId(j, "albedoPath");
        mat.heightMap = getAssetId(j, "heightmapPath");
        mat.metalMap = getAssetId(j, "metalMapPath");
        mat.roughMap = getAssetId(j, "roughMapPath");
        mat.normalMap = getAssetId(j, "normalMapPath");

        mat.cullOff = j.contains("cullOff");
        mat.wireframe = j.contains("wireframe");
        mat.usePBRMap = j.contains("pbrMapPath");
        mat.useAlphaTest = mat.alphaCutoff > 0.004f;

        if (mat.usePBRMap)
        {
            mat.pbrMap = getAssetId(j, "pbrMapPath");
        }
    }

    void MaterialEditor::draw()
    {
        ImGui::BeginChild("mat", ImVec2(0.0f, -ImGui::GetTextLineHeightWithSpacing() * 1.5f), false);
        ImGui::Columns(2);

        previewRegistry.get<WorldObject>(previewEntity).materials[0] = editingID;

        ImGui::BeginChild("materialSettings");
        unsavedChanges |= ImGui::SliderFloat("Metallic", &mat.metallic, 0.0f, 1.0f);
        unsavedChanges |= ImGui::SliderFloat("Roughness", &mat.roughness, 0.0f, 1.0f);

        if (mat.heightMap != ~0u)
            unsavedChanges |= ImGui::SliderFloat("Heightmap Scale", &mat.heightmapScale, 0.0f, 0.1f);

        unsavedChanges |= ImGui::ColorEdit3("Albedo Color", &mat.albedoColor.x);
        unsavedChanges |= ImGui::ColorEdit3("Emissive Color", &mat.emissiveColor.x,
                                            ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);

        // auto& texMan = interfaces.renderer->uiTextureManager();
        if (mat.albedo != ~0u)
        {
            ImGui::Text("Current albedo path: %s", AssetDB::idToPath(mat.albedo).c_str());
        }
        else
        {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Invalid albedo map");
        }
        unsavedChanges |= assetButton(mat.albedo, "Albedo");

        if (mat.normalMap != ~0u)
        {
            ImGui::Text("Current normal map path: %s", AssetDB::idToPath(mat.normalMap).c_str());
        }
        else
        {
            ImGui::Text("No normal map set");
        }
        unsavedChanges |= assetButton(mat.normalMap, "Normal map");

        if (mat.heightMap != ~0u)
        {
            ImGui::Text("Current height map path: %s", AssetDB::idToPath(mat.heightMap).c_str());
        }
        else
        {
            ImGui::Text("No height map set");
        }
        unsavedChanges |= assetButton(mat.heightMap, "Height map");

        unsavedChanges |= ImGui::Checkbox("Use packed PBR map", &mat.usePBRMap);
        if (mat.usePBRMap)
        {
            if (mat.pbrMap != ~0u)
            {
                ImGui::Text("Current PBR map path: %s", AssetDB::idToPath(mat.pbrMap).c_str());
            }
            else
            {
                ImGui::Text("No PBR map set");
            }

            unsavedChanges |= assetButton(mat.pbrMap, "PBR Map");
        }
        else
        {
            if (mat.metalMap != ~0u)
            {
                ImGui::Text("Current metallic map path: %s", AssetDB::idToPath(mat.metalMap).c_str());
            }
            else
            {
                ImGui::Text("No metallic map set");
            }

            unsavedChanges |= assetButton(mat.metalMap, "Metal Map");

            if (mat.roughMap != ~0u)
            {
                ImGui::Text("Current roughness map path: %s", AssetDB::idToPath(mat.roughMap).c_str());
            }
            else
            {
                ImGui::Text("No roughness map set");
            }

            unsavedChanges |= assetButton(mat.roughMap, "Rough Map");
        }

        unsavedChanges |= ImGui::Checkbox("Alpha Test", &mat.useAlphaTest);

        if (mat.useAlphaTest)
        {
            unsavedChanges |= ImGui::DragFloat("Alpha Cutoff", &mat.alphaCutoff);
        }

        if (ImGui::BeginPopup("Open Material from folder + mat name"))
        {
            static std::string folderPath = "Materials/";
            static std::string matName = "";

            ImGui::InputText("Folder", &folderPath);
            ImGui::InputText("Material Name", &matName);

            if (ImGui::Button("Set"))
            {
                std::string matPath = folderPath + matName;
                setIfExists(matPath + "_BaseColor.png", mat.albedo);
                setIfExists(matPath + "_Normal_forcelin.png", mat.normalMap);
                setIfExists(matPath + "_PBRPack.png", mat.pbrMap);

                setIfExists(matPath + "_BaseColor.wtex", mat.albedo);
                setIfExists(matPath + "_Normal_forcelin.wtex", mat.normalMap);
                setIfExists(matPath + "_PBRPack.wtex", mat.pbrMap);
                mat.usePBRMap = true;
                unsavedChanges = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (ImGui::Button("Set from folder + mat name"))
        {
            ImGui::OpenPopup("Open Material from folder + mat name");
        }
        ImGui::EndChild();

        ImGui::NextColumn();

        static ImVec2 lastPreviewSize{0.0f, 0.0f};
        ImVec2 previewSize =
            ImGui::GetContentRegionAvail() - ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 2.0f);
        bool resized = false;
        if (previewSize.x != lastPreviewSize.x || previewSize.y != lastPreviewSize.y)
        {
            rttPass->resize(std::max(16, (int)previewSize.x), std::max(16, (int)previewSize.y));
            lastPreviewSize = previewSize;
            resized = true;
            rttPass->active = true;
        }

        ImVec2 cpos = ImGui::GetWindowPos() + ImGui::GetCursorPos() - ImVec2(ImGui::GetScrollX(), ImGui::GetScrollY());
        ImVec2 end = previewSize + cpos;
        ImGui::ImageButton(rttPass->getUITextureID(), previewSize, ImVec2(0, 0), ImVec2(1, 1), 0);

        if (ImGui::IsMouseHoveringRect(cpos, end))
        {
            rttPass->active = true;
            if (ImGui::IsMouseDown(0))
            {
                dragging = true;
            }

            dist -= ImGui::GetIO().MouseWheel * 0.25;
            dist = glm::max(dist, 0.25f);
        }
        else if (!resized)
        {
            rttPass->active = false;
        }

        if (!ImGui::IsMouseDown(0))
        {
            dragging = false;
        }

        if (dragging)
        {
            lx -= ImGui::GetIO().MouseDelta.x * 0.01f;
            ly -= ImGui::GetIO().MouseDelta.y * 0.01f;

            ly = glm::clamp(ly, -glm::half_pi<float>() + 0.1f, glm::half_pi<float>() - 0.1f);
        }

        glm::quat q = glm::angleAxis(lx, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::angleAxis(ly, glm::vec3(1.0f, 0.0f, 0.0f));

        glm::vec3 dir = q * glm::vec3(0.0f, 0.0f, 1.0f);
        previewCam.position = dir * dist;
        previewCam.rotation = glm::quatLookAt(dir, glm::vec3(0.0f, 1.0f, 0.0f));

        if (ImGui::Button("Box"))
        {
            previewRegistry.get<WorldObject>(previewEntity).mesh = AssetDB::pathToId("Models/cube.wmdl");
            rttPass->active = true;
        }

        ImGui::SameLine();
        if (ImGui::Button("Sphere"))
        {
            previewRegistry.get<WorldObject>(previewEntity).mesh = AssetDB::pathToId("Models/sphere.wmdl");
            rttPass->active = true;
        }

        ImGui::SameLine();
        if (ImGui::Button("Plane"))
        {
            previewRegistry.get<WorldObject>(previewEntity).mesh = AssetDB::pathToId("Models/plane.wmdl");
            rttPass->active = true;
        }

        static AssetID customModel = ~0u;
        ImGui::SameLine();
        bool openModelPopup = false;
        if (ImGui::Button("Other Model"))
        {
            openModelPopup = true;
        }

        ImGui::SameLine();

        bool openSkyboxPopup = false;
        auto& sceneSettings = previewRegistry.ctx<SkySettings>();

        if (ImGui::Button("Change Background"))
        {
            openSkyboxPopup = true;
        }

        if (selectAssetPopup("Preview Model", customModel, openModelPopup))
        {
            previewRegistry.get<WorldObject>(previewEntity).mesh = customModel;
        }

        selectAssetPopup("Skybox", sceneSettings.skybox, openSkyboxPopup);

        ImGui::EndColumns();
        ImGui::EndChild();
    }

    void MaterialEditor::save()
    {
        saveMaterial(mat, editingID);
        // interfaces.renderer->reloadContent(ReloadFlags::Materials);
        rttPass->active = true;
        unsavedChanges = false;
    }

    bool MaterialEditor::hasUnsavedChanges()
    {
        return unsavedChanges;
    }

    MaterialEditor::~MaterialEditor()
    {
        EngineInterfaces interfaces = this->interfaces;
        RTTPass* rPass = rttPass;
        rPass->active = false;
        // for (auto& p : cacheTextures) {
        //    interfaces.renderer->uiTextureManager().unload(p.first);
        //}

        cacheTextures.clear();
        interfaces.renderer->destroyRTTPass(rPass);
    }

    void MaterialEditorMeta::importAsset(std::string filePath, std::string newAssetPath)
    {
        logErr("You can't import materials! You will regret this!");
    }

    void MaterialEditorMeta::create(std::string path)
    {
        PHYSFS_File* f = PHYSFS_openWrite(path.c_str());
        const char emptyJson[] = "{}";
        PHYSFS_writeBytes(f, emptyJson, sizeof(emptyJson));
        PHYSFS_close(f);
    }

    IAssetEditor* MaterialEditorMeta::createEditorFor(AssetID id)
    {
        return new MaterialEditor(id, interfaces);
    }

    const char* MaterialEditorMeta::getHandledExtension()
    {
        return ".json";
    }
}
