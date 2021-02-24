#include "GuiUtil.hpp"
#include "../Core/Engine.hpp"
#include "../Libs/IconsFontAwesome5.h"
#include "../Libs/IconsFontaudio.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "../ImGui/imgui_internal.h"
#include <filesystem>

namespace worlds {
    const char* getIcon(std::string extension) {
        if (extension == ".escn" || extension == ".wscn") {
            return (const char*)(ICON_FA_MAP u8" ");
        } else if (extension == ".ogg") {
            return (const char*)(ICON_FAD_SPEAKER u8" ");
        } else if (extension == ".crn") {
            return (const char*)(ICON_FA_IMAGE u8" ");
        } else if (extension == ".obj" || extension == ".wmdl" || extension == ".mdl") {
            return (const char*)(ICON_FA_SHAPES u8" ");
        }
        return "      ";
    }

    void saveFileModalold(const char* title, std::function<void(const char*)> saveCallback) {
        ImVec2 popupSize(windowSize.x - 50.0f, windowSize.y - 50.0f);
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->Pos + ImVec2((windowSize.x / 2.0f) - (popupSize.x / 2), (windowSize.y / 2.0f) - (popupSize.y / 2)));
        ImGui::SetNextWindowSize(popupSize);

        if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
            std::string* pathPtr = (std::string*)ImGui::GetStateStorage()->GetVoidPtr(ImGui::GetID("savepath"));
            std::string* saveDir = (std::string*)ImGui::GetStateStorage()->GetVoidPtr(ImGui::GetID("savedir"));

            if (pathPtr == nullptr) {
                pathPtr = new std::string;
                ImGui::GetStateStorage()->SetVoidPtr(ImGui::GetID("savepath"), pathPtr);
            }

            if (saveDir == nullptr) {
                saveDir = new std::string;
                ImGui::GetStateStorage()->SetVoidPtr(ImGui::GetID("savedir"), saveDir);
            }

            ImGui::Text("%s", saveDir->c_str());

            ImGui::BeginChild("Stuffs", ImVec2(ImGui::GetWindowWidth() - 17.0f, ImGui::GetWindowHeight() - 90.0f), true);
            char** files = PHYSFS_enumerateFiles(saveDir->c_str());

            if (!saveDir->empty()) {
                ImGui::Text("..");
                if (ImGui::IsItemClicked()) {
                    std::filesystem::path p(*saveDir);
                    *saveDir = p.parent_path().string();
                }
            }

            for (char** currFile = files; *currFile != nullptr; currFile++) {
                PHYSFS_Stat stat;
                PHYSFS_stat(*currFile, &stat);
                ImGui::Text("%s%s", *currFile, stat.filetype == PHYSFS_FILETYPE_DIRECTORY ? "/" : "");

                if (ImGui::IsItemClicked()) {
                    logMsg("clicked filetype of %u", stat.filetype);
                    if (stat.filetype == PHYSFS_FILETYPE_REGULAR) {
                        *pathPtr = *currFile;
                    } else {
                        *saveDir = *currFile;
                    }
                }
            }
            ImGui::EndChild();

            std::string titleConfirm = "Confirm##";
            titleConfirm += title;

            ImGui::PushItemWidth(ImGui::GetWindowWidth() - 109.0f);
            ImGui::InputText("", pathPtr);

            std::filesystem::path filePath = *saveDir;
            filePath.append(*pathPtr);

            ImGui::SameLine();
            if (ImGui::Button("OK") && !pathPtr->empty()) {
                if (PHYSFS_exists(filePath.string().c_str())) {
                    ImGui::OpenPopup(titleConfirm.c_str());
                } else {
                    saveCallback(filePath.string().c_str());
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                ImGui::CloseCurrentPopup();

            if (ImGui::BeginPopup(titleConfirm.c_str())) {
                ImGui::Text("This file already exists. Do you want to overwrite?");

                if (ImGui::Button("Overwrite")) {
                    saveCallback(filePath.string().c_str());
                    ImGui::ClosePopupToLevel(0, true);
                }
                ImGui::EndPopup();
            }

            ImGui::EndPopup();
        }
    }

    void saveFileModal(const char* title, std::function<void(const char*)> saveCallback) {
        ImVec2 popupSize(windowSize.x - 50.0f, windowSize.y - 50.0f);
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->Pos + ImVec2((windowSize.x / 2) - (popupSize.x / 2), (windowSize.y / 2) - (popupSize.y / 2)));
        ImGui::SetNextWindowSize(popupSize);

        if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
            std::string* fullFilePath = (std::string*)ImGui::GetStateStorage()->GetVoidPtr(ImGui::GetID("savepath"));
            std::string* currentDirectory = (std::string*)ImGui::GetStateStorage()->GetVoidPtr(ImGui::GetID("savedir"));

            if (fullFilePath == nullptr) {
                fullFilePath = new std::string;
                ImGui::GetStateStorage()->SetVoidPtr(ImGui::GetID("savepath"), fullFilePath);
            }

            if (currentDirectory == nullptr) {
                currentDirectory = new std::string;
                ImGui::GetStateStorage()->SetVoidPtr(ImGui::GetID("savedir"), currentDirectory);
            }

            ImGui::Text("%s", currentDirectory->c_str());

            ImGui::BeginChild("Stuffs", ImVec2(ImGui::GetWindowWidth() - 17.0f, ImGui::GetWindowHeight() - 90.0f), true);
            char** files = PHYSFS_enumerateFiles(currentDirectory->c_str());

            std::string titleDoesntExist = "File Doesn't Exist##";
            titleDoesntExist += title;

            if (!currentDirectory->empty()) {
                ImGui::Text("..");
                if (ImGui::IsItemClicked()) {
                    std::filesystem::path p(*currentDirectory);
                    *currentDirectory = p.parent_path().string();
                }
            }

            for (char** currFile = files; *currFile != nullptr; currFile++) {
                std::string absPath = currentDirectory->empty() ? *currFile : (*currentDirectory + "/" + (*currFile));
                PHYSFS_Stat stat;
                PHYSFS_stat(absPath.c_str(), &stat);
                std::string extension = std::filesystem::path(*currFile).extension().string();

                const char* icon = "  ";

                if (stat.filetype == PHYSFS_FILETYPE_DIRECTORY) {
                    icon = (const char*)(ICON_FA_FOLDER u8" ");
                } else icon = getIcon(extension);
                
                ImGui::Text("%s%s%s", icon, *currFile, stat.filetype == PHYSFS_FILETYPE_DIRECTORY ? "/" : "");

                if (ImGui::IsItemClicked()) {
                    if (stat.filetype == PHYSFS_FILETYPE_REGULAR) {
                        *fullFilePath = absPath;
                        logMsg("selected %s", fullFilePath->c_str());
                    } else {
                        if (*currentDirectory != "/")
                            *currentDirectory += "/";
                        *currentDirectory += *currFile;

                        if ((*currentDirectory)[0] == '/') {
                            *currentDirectory = currentDirectory->substr(1);
                        }

                        logMsg("navigated to %s", currentDirectory->c_str());
                    }
                }

                if (ImGui::IsItemHovered() && stat.filetype == PHYSFS_FILETYPE_REGULAR) {
                    if (ImGui::IsMouseDoubleClicked(0)) {
                        saveCallback(absPath.c_str());
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
            ImGui::EndChild();

            ImGui::PushItemWidth(ImGui::GetWindowWidth() - 109.0f);
            ImGui::InputText("", fullFilePath);

            ImGui::SameLine();
            if (ImGui::Button("OK") && !fullFilePath->empty()) {
                if (!PHYSFS_exists(fullFilePath->c_str())) {
                    ImGui::OpenPopup(titleDoesntExist.c_str());
                } else {
                    saveCallback(fullFilePath->c_str());
                    ImGui::GetStateStorage()->Clear();
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::GetStateStorage()->Clear();
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    } 

    void openFileModal(const char* title, std::function<void(const char*)> openCallback, const char** fileExtensions, int fileExtensionCount, const char* startingDir) {
        ImVec2 popupSize(windowSize.x - 50.0f, windowSize.y - 50.0f);
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->Pos + ImVec2((windowSize.x / 2) - (popupSize.x / 2), (windowSize.y / 2) - (popupSize.y / 2)));
        ImGui::SetNextWindowSize(popupSize);

        if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
            std::string* fullFilePath = (std::string*)ImGui::GetStateStorage()->GetVoidPtr(ImGui::GetID("savepath"));
            std::string* currentDirectory = (std::string*)ImGui::GetStateStorage()->GetVoidPtr(ImGui::GetID("savedir"));

            if (fullFilePath == nullptr) {
                fullFilePath = new std::string;
                ImGui::GetStateStorage()->SetVoidPtr(ImGui::GetID("savepath"), fullFilePath);
            }

            if (currentDirectory == nullptr) {
                currentDirectory = new std::string;
                if (startingDir)
                    *currentDirectory = startingDir;
                ImGui::GetStateStorage()->SetVoidPtr(ImGui::GetID("savedir"), currentDirectory);
            }

            ImGui::Text("%s", currentDirectory->c_str());

            ImGui::BeginChild("Stuffs", ImVec2(ImGui::GetWindowWidth() - 17.0f, ImGui::GetWindowHeight() - 90.0f), true);
            char** files = PHYSFS_enumerateFiles(currentDirectory->c_str());

            std::string titleDoesntExist = "File Doesn't Exist##";
            titleDoesntExist += title;

            if (!currentDirectory->empty()) {
                ImGui::Text("..");
                if (ImGui::IsItemClicked()) {
                    std::filesystem::path p(*currentDirectory);
                    *currentDirectory = p.parent_path().string();
                }
            }

            for (char** currFile = files; *currFile != nullptr; currFile++) {
                std::string absPath = currentDirectory->empty() ? *currFile : (*currentDirectory + "/" + (*currFile));
                PHYSFS_Stat stat;
                PHYSFS_stat(absPath.c_str(), &stat);
                std::string extension = std::filesystem::path(*currFile).extension().string();

                bool hasExtension = fileExtensionCount == 0;

                for (int i = 0; i < fileExtensionCount; i++) {
                    if (fileExtensions[i] == extension)
                        hasExtension = true;
                } 

                if (!hasExtension && stat.filetype == PHYSFS_FILETYPE_REGULAR)
                    continue;

                const char* icon = "  ";

                if (stat.filetype == PHYSFS_FILETYPE_DIRECTORY) {
                    icon = (const char*)(ICON_FA_FOLDER u8" ");
                } else icon = getIcon(extension);
                
                ImGui::Text("%s%s%s", icon, *currFile, stat.filetype == PHYSFS_FILETYPE_DIRECTORY ? "/" : "");

                if (ImGui::IsItemClicked()) {
                    if (stat.filetype == PHYSFS_FILETYPE_REGULAR) {
                        *fullFilePath = absPath;
                        logMsg("selected %s", fullFilePath->c_str());
                    } else {
                        if (*currentDirectory != "/")
                            *currentDirectory += "/";
                        *currentDirectory += *currFile;

                        if ((*currentDirectory)[0] == '/') {
                            *currentDirectory = currentDirectory->substr(1);
                        }

                        logMsg("navigated to %s", currentDirectory->c_str());
                    }
                }

                if (ImGui::IsItemHovered() && stat.filetype == PHYSFS_FILETYPE_REGULAR) {
                    if (ImGui::IsMouseDoubleClicked(0)) {
                        if (!PHYSFS_exists(absPath.c_str())) {
                            ImGui::OpenPopup(titleDoesntExist.c_str());
                        } else {
                            openCallback(absPath.c_str());
                            ImGui::CloseCurrentPopup();
                        }
                    }
                }
            }
            ImGui::EndChild();

            ImGui::PushItemWidth(ImGui::GetWindowWidth() - 109.0f);
            ImGui::InputText("", fullFilePath);

            ImGui::SameLine();
            if (ImGui::Button("OK") && !fullFilePath->empty()) {
                if (!PHYSFS_exists(fullFilePath->c_str())) {
                    ImGui::OpenPopup(titleDoesntExist.c_str());
                } else {
                    openCallback(fullFilePath->c_str());
                    ImGui::GetStateStorage()->Clear();
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::GetStateStorage()->Clear();
                ImGui::CloseCurrentPopup();
            }

            if (ImGui::BeginPopup(titleDoesntExist.c_str())) {
                ImGui::Text("File not found.");
                ImGui::EndPopup();
            }

            ImGui::EndPopup();
        }
    }

    void openFileModal(const char* title, std::function<void(const char*)> openCallback, const char* fileExtension, const char* startingDir) {
        int extCount = fileExtension != nullptr;
        openFileModal(title, openCallback, &fileExtension, extCount, startingDir);
    }

    bool selectAssetPopup(const char* title, AssetID& id, bool open) {
        static std::string path;
        static bool notFoundErr = false;
        bool changed = false;

        std::filesystem::path p(path);
        p = p.parent_path();

        openFileModal(title, [&](const char* path) {
            id = g_assetDB.addOrGetExisting(path);
            changed = true;
        }, nullptr, p.string().c_str());

        if (open) {
            path = g_assetDB.getAssetPath(id);
            ImGui::OpenPopup(title);
        }

        return changed;
    }

    // Based on code from the ImGui demo
    void tooltipHover(const char* desc) {
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 25.0f);
            ImGui::TextUnformatted(desc);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }
}
