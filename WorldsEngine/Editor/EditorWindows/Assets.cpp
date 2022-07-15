#include "EditorWindows.hpp"
#include <AssetCompilation/AssetCompilers.hpp>
#include <Core/Console.hpp>
#include <Core/Log.hpp>
#include <Editor/AssetEditors.hpp>
#include <Editor/GuiUtil.hpp>
#include <ImGui/imgui.h>
#include <Libs/IconsFontAwesome5.h>
#include <Serialization/SceneSerialization.hpp>
#include <Util/CreateModelObject.hpp>
#include <filesystem>
#include <slib/Path.hpp>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#elif defined(__linux__)
#endif
#include <AssetCompilation/AssetCompilerUtil.hpp>

namespace worlds
{
    void Assets::draw(entt::registry &reg)
    {
        static std::string currentDir = "";

        static ConVar showExts{"editor_assetExtDbg", "0", "Shows parsed file extensions in brackets."};
        if (ImGui::Begin(ICON_FA_FOLDER u8" Assets", &active))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
            ImGui::InputText("###lol", &currentDir);
            ImGui::PopStyleVar(2);

            if (!currentDir.empty())
            {
                ImGui::SameLine();
                if (ImGui::Button(ICON_FA_ARROW_UP))
                {
                    std::filesystem::path p{currentDir};
                    currentDir = p.parent_path().string();
                    if (currentDir == "/")
                        currentDir = "";

                    if (currentDir[0] == '/')
                    {
                        currentDir = currentDir.substr(1);
                    }
                    logMsg("Navigated to %s", currentDir.c_str());
                }
            }

            ImGui::Separator();

            char **files = PHYSFS_enumerateFiles(("SourceData/" + currentDir).c_str());

            if (*files == nullptr)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Invalid path");

                if (currentDir.find('\\') != std::string::npos)
                {
                    ImGui::Text("(Paths should use forward slashes rather than backslashes)");
                }
            }
            std::string assetContextMenuPath;

            for (char **currFile = files; *currFile != nullptr; currFile++)
            {
                slib::Path path{*currFile};
                std::string origDirStr = "SourceData/" + currentDir;
                if (origDirStr[0] == '/')
                {
                    origDirStr = origDirStr.substr(1);
                }

                std::string fullPath;

                if (origDirStr.empty())
                    fullPath = *currFile;
                else
                    fullPath = origDirStr + "/" + std::string(*currFile);

                PHYSFS_Stat stat;
                PHYSFS_stat(fullPath.c_str(), &stat);

                if (stat.filetype == PHYSFS_FILETYPE_DIRECTORY || stat.filetype == PHYSFS_FILETYPE_SYMLINK)
                {
                    slib::String buttonLabel{(const char *)ICON_FA_FOLDER};
                    buttonLabel += " ";
                    buttonLabel += *currFile;
                    if (ImGui::Button(buttonLabel.cStr()))
                    {
                        if (currentDir != "/")
                            currentDir += "/";
                        currentDir += *currFile;

                        if (currentDir[0] == '/')
                        {
                            currentDir = currentDir.substr(1);
                        }
                        logMsg("Navigated to %s", currentDir.c_str());
                    }
                }
                else
                {
                    slib::Path p{fullPath.c_str()};
                    auto ext = p.fileExtension();
                    const char *icon = getIcon(ext.cStr());
                    slib::String buttonLabel = icon;
                    buttonLabel += *currFile;

                    ImGui::Text("%s", buttonLabel.cStr());

                    if (ImGui::IsItemHovered())
                    {
                        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        {
                            editor->openAsset(AssetDB::pathToId(fullPath));
                        }

                        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                        {
                            assetContextMenuPath = fullPath;
                        }
                    }
                }
            }

            static IAssetEditorMeta *newAssetEditor = nullptr;
            static std::string newAssetName;

            if (ImGui::BeginPopup("New Asset Name"))
            {
                if (ImGui::InputText("Name", &newAssetName, ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    std::string newAssetPath = "SourceData/" + currentDir + "/" + newAssetName;
                    logMsg("Creating new asset in %s", newAssetPath.c_str());
                    newAssetEditor->create(newAssetPath);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            if (ImGui::BeginPopup("New Folder"))
            {
                static std::string newFolderName;
                if (ImGui::InputText("Folder Name", &newFolderName, ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    std::string newFolderPath = "SourceData/" + currentDir + "/" + newFolderName;
                    std::filesystem::create_directories(newFolderPath);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            if (ImGui::BeginPopup("New Material"))
            {
                static std::string newMaterialName;
                if (ImGui::InputText("Name", &newMaterialName, ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    std::string p = "SourceData/" + currentDir + "/" + newMaterialName;
                    PHYSFS_File *f = PHYSFS_openWrite(p.c_str());
                    const char emptyJson[] = "{}";
                    PHYSFS_writeBytes(f, emptyJson, sizeof(emptyJson));
                    PHYSFS_close(f);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            static std::string assetContextMenu;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && ImGui::IsWindowHovered())
            {
                assetContextMenu = assetContextMenuPath;
                ImGui::OpenPopup("ContextMenu");
            }

            bool openAssetNamePopup = false;
            bool openNewFolderPopup = false;
            bool openNMaterialPopup = false;
            if (ImGui::BeginPopup("ContextMenu"))
            {
                if (ImGui::Button("New Folder"))
                {
                    ImGui::CloseCurrentPopup();
                    openNewFolderPopup = true;
                }

                if (ImGui::Button("Create Material"))
                {
                    ImGui::CloseCurrentPopup();
                    openNMaterialPopup = true;
                }

                std::string assetExtension = std::filesystem::path{assetContextMenu}.extension().string();

                if (assetExtension == ".wmdlj")
                {
                    if (ImGui::Button("Create in scene"))
                    {
                        AssetID compiledAsset = getOutputAsset(assetContextMenu);
                        Camera &editorCam = editor->getFirstSceneView()->getCamera();
                        glm::vec3 pos = editorCam.position + editorCam.rotation * glm::vec3(0.0f, 0.0f, 1.0f);
                        createModelObject(reg, pos, glm::quat{1.0f, 0.0f, 0.0f, 0.0f}, compiledAsset,
                                          AssetDB::pathToId("Materials/DevTextures/dev_blue.json"));
                    }
                }

#ifdef _WIN32
                if (ImGui::Button("Open Folder in Explorer"))
                {
                    ImGui::CloseCurrentPopup();
                    std::string rawAssetDir =
                        std::string(editor->currentProject().root()) + "/SourceData/" + currentDir;
                    logMsg("opening %s", rawAssetDir.c_str());
                    ShellExecuteA(nullptr, "open", rawAssetDir.c_str(), nullptr, nullptr, SW_SHOW);
                }
#elif defined(__linux__)
                if (ImGui::Button("Open Folder in File Manager"))
                {
                    std::string cmd = std::string("xdg-open ") + std::string(editor->currentProject().root()) +
                                      "/SourceData/" + currentDir;
                    system(cmd.c_str());
                }
#endif

                for (size_t i = 0; i < AssetCompilers::registeredCompilerCount(); i++)
                {
                    IAssetCompiler *compiler = AssetCompilers::registeredCompilers()[i];
                    if (ImGui::Button(compiler->getSourceExtension()))
                    {
                        newAssetEditor = AssetEditors::getEditorFor(compiler->getSourceExtension());
                        newAssetName = std::string("New Asset") + compiler->getSourceExtension();
                        ImGui::CloseCurrentPopup();
                        openAssetNamePopup = true;
                    }
                }
                ImGui::EndPopup();
            }

            if (openAssetNamePopup)
                ImGui::OpenPopup("New Asset Name");

            if (openNewFolderPopup)
                ImGui::OpenPopup("New Folder");

            if (openNMaterialPopup)
                ImGui::OpenPopup("New Material");

            PHYSFS_freeList(files);
        }

        ImGui::End();
    }
}
