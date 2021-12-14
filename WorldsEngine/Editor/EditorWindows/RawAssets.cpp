#include "EditorWindows.hpp"
#include <ImGui/imgui.h>
#include <filesystem>
#include <Util/CreateModelObject.hpp>
#include <Core/Console.hpp>
#include <Libs/IconsFontAwesome5.h>
#include <Editor/GuiUtil.hpp>
#include <slib/Path.hpp>
#include <AssetCompilation/AssetCompilers.hpp>
#include <Editor/AssetEditors.hpp>
#include <Serialization/SceneSerialization.hpp>
#include <Core/Log.hpp>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h>
#elif defined(__linux__)
#endif

namespace worlds {
    void RawAssets::draw(entt::registry& reg) {
        static std::string currentDir = "";

        static ConVar showExts{ "editor_rawAssetExtDbg", "0", "Shows parsed file extensions in brackets." };
        if (ImGui::Begin(ICON_FA_FOLDER u8" Raw Assets", &active)) {
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
            ImGui::InputText("###lol", &currentDir);
            ImGui::PopStyleVar(2);

            if (!currentDir.empty()) {
                ImGui::SameLine();
                if (ImGui::Button(ICON_FA_ARROW_UP)) {
                    std::filesystem::path p{ currentDir };
                    currentDir = p.parent_path().string();
                    if (currentDir == "/")
                        currentDir = "";

                    if (currentDir[0] == '/') {
                        currentDir = currentDir.substr(1);
                    }
                    logMsg("Navigated to %s", currentDir.c_str());
                }
            }

            ImGui::Separator();

            char** files = PHYSFS_enumerateFiles(("Raw/" + currentDir).c_str());

            if (*files == nullptr) {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Invalid path");

                if (currentDir.find('\\') != std::string::npos) {
                    ImGui::Text("(Paths should use forward slashes rather than backslashes)");
                }
            }
            std::string assetContextMenuPath;

            for (char** currFile = files; *currFile != nullptr; currFile++) {
                slib::Path path{ *currFile };
                std::string origDirStr = "Raw/" + currentDir;
                if (origDirStr[0] == '/') {
                    origDirStr = origDirStr.substr(1);
                }

                std::string fullPath;

                if (origDirStr.empty())
                    fullPath = *currFile;
                else
                    fullPath = origDirStr + "/" + std::string(*currFile);

                PHYSFS_Stat stat;
                PHYSFS_stat(fullPath.c_str(), &stat);

                if (stat.filetype == PHYSFS_FILETYPE_DIRECTORY || stat.filetype == PHYSFS_FILETYPE_SYMLINK) {
                    slib::String buttonLabel{ (const char*)ICON_FA_FOLDER };
                    buttonLabel += " ";
                    buttonLabel += *currFile;
                    if (ImGui::Button(buttonLabel.cStr())) {
                        if (currentDir != "/")
                            currentDir += "/";
                        currentDir += *currFile;

                        if (currentDir[0] == '/') {
                            currentDir = currentDir.substr(1);
                        }
                        logMsg("Navigated to %s", currentDir.c_str());
                    }
                } else {
                    slib::Path p{ fullPath.c_str() };
                    auto ext = p.fileExtension();
                    const char* icon = getIcon(ext.cStr());
                    slib::String buttonLabel = icon;
                    buttonLabel += *currFile;

                    ImGui::Text("%s", buttonLabel.cStr());

                    if (ImGui::IsItemHovered()) {
                        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                            if (ext == ".wscn") {
                                interfaces.engine->loadScene(AssetDB::pathToId(fullPath));
                            } else if (ext == ".wprefab") {
                                SceneLoader::createPrefab(AssetDB::pathToId(fullPath), reg);
                            } else {
                                editor->currentSelectedAsset = AssetDB::pathToId(fullPath);
                            }
                        }

                        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                            assetContextMenuPath = fullPath;
                        }
                    }
                }
            }

            static IAssetEditor* newAssetEditor = nullptr;
            static std::string newAssetName;

            if (ImGui::BeginPopup("New Asset Name")) {
                if (ImGui::InputText("Name", &newAssetName, ImGuiInputTextFlags_EnterReturnsTrue)) {
                    std::string newAssetPath = "Raw/" + currentDir + "/" + newAssetName;
                    logMsg("Creating new asset in %s", newAssetPath.c_str());
                    newAssetEditor->create(newAssetPath);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            if (ImGui::BeginPopup("New Folder")) {
                static std::string newFolderName;
                if (ImGui::InputText("Folder Name", &newFolderName, ImGuiInputTextFlags_EnterReturnsTrue)) {
                    std::string newFolderPath = "Raw/" + currentDir + "/" + newFolderName;
                    std::filesystem::create_directories(newFolderPath);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            static std::string assetContextMenu;
            static bool isTextureFolder = false;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && ImGui::IsWindowHovered()) {
                assetContextMenu = assetContextMenuPath;
                ImGui::OpenPopup("ContextMenu");

                isTextureFolder = true;
                for (char** currFile = files; *currFile != nullptr; currFile++) {
                    slib::Path path{ *currFile };
                    std::string origDirStr = "Raw/" + currentDir;
                    if (origDirStr[0] == '/') {
                        origDirStr = origDirStr.substr(1);
                    }

                    std::string fullPath;

                    if (origDirStr.empty())
                        fullPath = *currFile;
                    else
                        fullPath = origDirStr + "/" + std::string(*currFile);

                    PHYSFS_Stat stat;
                    PHYSFS_stat(fullPath.c_str(), &stat);

                    if (stat.filetype == PHYSFS_FILETYPE_REGULAR) {
                        bool isFileTexture = false;
                        std::array<const char*, 3> textureExtensions = { ".png", ".jpg", ".tga" };

                        for (const char* ext : textureExtensions) {
                            if (path.fileExtension() == ext) {
                                isFileTexture = true;
                            }
                        }

                        isTextureFolder &= isFileTexture;
                    }
                }
            }

            bool openAssetNamePopup = false;
            bool openNewFolderPopup = false;
            if (ImGui::BeginPopup("ContextMenu")) {
                if (!assetContextMenu.empty()) {
                    slib::Path p{ assetContextMenu.c_str() };
                    auto ext = p.fileExtension();

                    if (ext == ".png" || ext == ".jpg") {
                        if (ImGui::Button("Create corresponding wtex")) {
                            std::string path = assetContextMenu;
                            std::string removeStr = "Raw/";

                            size_t pos = path.find(removeStr);
                            if (pos != std::string::npos) {
                                path.erase(pos, removeStr.size());
                                pos = path.find(ext.cStr());
                                path.erase(pos, ext.byteLength());
                                path += ".wtexj";
                                std::filesystem::create_directory(std::filesystem::path{ path }.parent_path());
                                AssetEditors::getEditorFor(".wtexj")->importAsset(assetContextMenu, path);
                            }
                        }
                    }

                    if (ext == ".fbx") {
                        if (ImGui::Button("Import model")) {
                            std::string path = assetContextMenu;
                            std::string removeStr = "Raw/";

                            size_t pos = path.find(removeStr);
                            if (pos != std::string::npos) {
                                path.erase(pos, removeStr.size());
                                pos = path.find(ext.cStr());
                                path.erase(pos, ext.byteLength());
                                path += ".wmdlj";
                                std::filesystem::create_directory(std::filesystem::path{ path }.parent_path());
                                AssetEditors::getEditorFor(".wmdlj")->importAsset(assetContextMenu, path);
                            }
                        }
                    }
                    ImGui::Separator();
                }

                if (isTextureFolder) {
                    if (ImGui::Button("Import folder")) {
                        for (char** currFile = files; *currFile != nullptr; currFile++) {
                            slib::Path path{ *currFile };
                            std::string origDirStr = currentDir;
                            if (origDirStr[0] == '/') {
                                origDirStr = origDirStr.substr(1);
                            }

                            std::string fullPath = "Raw/";

                            if (origDirStr.empty())
                                fullPath += *currFile;
                            else
                                fullPath += origDirStr + "/" + std::string(*currFile);

                            PHYSFS_Stat stat;
                            PHYSFS_stat(fullPath.c_str(), &stat);

                            if (stat.filetype == PHYSFS_FILETYPE_REGULAR) {
                                std::string strPath = fullPath;
                                std::string removeStr = "Raw/";

                                size_t pos = strPath.find(removeStr);
                                if (pos != std::string::npos) {
                                    slib::String ext = path.fileExtension();
                                    strPath.erase(pos, removeStr.size());
                                    pos = strPath.find(ext.cStr());
                                    strPath.erase(pos, ext.byteLength());
                                    strPath += ".wtexj";

                                    strPath = "SourceData/" + strPath;

                                    std::filesystem::path realPath = editor->currentProject().root();
                                    realPath /= strPath;
                                    realPath = realPath.lexically_normal();

                                    std::filesystem::create_directories(realPath.parent_path());
                                    AssetEditors::getEditorFor(".wtexj")->importAsset(fullPath, strPath);
                                }
                            }
                        }

                    }
                    ImGui::Separator();
                }

                if (ImGui::Button("New Folder")) {
                    ImGui::CloseCurrentPopup();
                    openNewFolderPopup = true;
                }

#ifdef _WIN32
                if (ImGui::Button("Open Folder in Explorer")) {
                    ImGui::CloseCurrentPopup();
                    std::string rawAssetDir = std::string(editor->currentProject().root()) + "/Raw/" + currentDir;
                    logMsg("opening %s", rawAssetDir.c_str());
                    ShellExecuteA(nullptr, "open", rawAssetDir.c_str(), nullptr, nullptr, SW_SHOW);
                }
#elif defined(__linux__)
                if (ImGui::Button("Open Folder in File Manager")) {
                    std::string cmd = std::string("xdg-open ") + std::string(editor->currentProject().root()) + "/Raw/" + currentDir;
                    system(cmd.c_str());
                }
#endif

                for (size_t i = 0; i < AssetCompilers::registeredCompilerCount(); i++) {
                    IAssetCompiler* compiler = AssetCompilers::registeredCompilers()[i];
                    if (ImGui::Button(compiler->getSourceExtension())) {
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

            PHYSFS_freeList(files);
        }

        ImGui::End();
    }
}
