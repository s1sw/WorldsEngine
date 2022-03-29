#include "GuiUtil.hpp"
#include "../Core/Engine.hpp"
#include "../Libs/IconsFontAwesome5.h"
#include "../Libs/IconsFontaudio.h"
#include "ImGui/imgui.h"
#include "robin_hood.h"
#include <string>
#define IMGUI_DEFINE_MATH_OPERATORS
#include "../ImGui/imgui_internal.h"
#include "../Core/Log.hpp"
#include <filesystem>
#ifdef _WIN32
#include <slib/Win32Util.hpp>
#endif

namespace worlds {
    ImFont* boldFont;
    const char8_t* ICONSTR_MODEL = ICON_FA_SHAPES u8" ";
    const char8_t* ICONSTR_SCENE = ICON_FA_MAP u8" ";
    const char8_t* ICONSTR_TEXTURE = ICON_FA_IMAGE u8" ";

    const robin_hood::unordered_map<std::string, const char8_t*> extensionIcons = {
        { ".wscn", ICONSTR_SCENE },
        { ".wtexj", ICONSTR_TEXTURE },
        { ".wmdlj", ICONSTR_MODEL },

        { ".wmdl", ICONSTR_MODEL },
        { ".wtex", ICONSTR_TEXTURE },

        { ".png", ICONSTR_TEXTURE },
        { ".jpg", ICONSTR_TEXTURE },
        { ".tga", ICONSTR_TEXTURE },

        { ".fbx", ICONSTR_MODEL },
        { ".obj", ICONSTR_MODEL },
        { ".glb", ICONSTR_MODEL }
    };

    const char* getIcon(const std::string& extension) {
        if (extensionIcons.contains(extension)) {
            return reinterpret_cast<const char*>(extensionIcons.at(extension));
        }

        return "      ";
    }

    void listDirectoryItems(std::string path,
        std::function<void(std::string, PHYSFS_FileType)> onClick,
        std::function<void(std::string, PHYSFS_FileType)> onDoubleClick) {
        if (!path.empty()) {
            ImGui::Text("..");
            if (ImGui::IsItemClicked()) {
                std::filesystem::path p(path);
                onDoubleClick(p.parent_path().string(), PHYSFS_FILETYPE_DIRECTORY);
            }
        }

        char** files = PHYSFS_enumerateFiles(path.c_str());

        std::vector<char*> fileVec;
        for (char** currFile = files; *currFile != nullptr; currFile++) {
            fileVec.push_back(*currFile);
        }

        std::sort(fileVec.begin(), fileVec.end(), [&](const char* pathA, const char* pathB) {
            std::string absPathA = path.empty() ? pathA : (path + "/" + pathA);
            PHYSFS_Stat statA;
            PHYSFS_stat(absPathA.c_str(), &statA);
            std::string extensionA = std::filesystem::path(pathA).extension().string();

            std::string absPathB = path.empty() ? pathB : (path + "/" + pathB);
            PHYSFS_Stat statB;
            PHYSFS_stat(absPathB.c_str(), &statB);
            std::string extensionB = std::filesystem::path(pathB).extension().string();

            if (extensionA < extensionB) return true;
            if (extensionB > extensionA) return false;

            int cmpResult = strcmp(pathA, pathB);

            if (cmpResult < 0) return true;
            if (cmpResult > 0) return false;
            
            return false;
            });

        for (auto currFile : fileVec) {
            std::string absPath = path.empty() ? currFile : (path + "/" + (currFile));
            PHYSFS_Stat stat;
            PHYSFS_stat(absPath.c_str(), &stat);
            std::string extension = std::filesystem::path(currFile).extension().string();

            const char* icon = "  ";

            if (stat.filetype == PHYSFS_FILETYPE_DIRECTORY) {
                icon = (const char*)(ICON_FA_FOLDER u8" ");
            } else icon = getIcon(extension);

            ImGui::Text("%s%s%s", icon, currFile, stat.filetype == PHYSFS_FILETYPE_DIRECTORY ? "/" : "");

            if (ImGui::IsItemClicked()) {
                if (stat.filetype == PHYSFS_FILETYPE_REGULAR) {
                    onClick(absPath, stat.filetype);
                    logMsg("selected %s", absPath.c_str());
                } else {
                    logMsg("navigated to %s", absPath.c_str());
                }
            }

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                onDoubleClick(absPath, stat.filetype);
            }
        }

        PHYSFS_freeList(files);
    }

    ImVec2 getPopupPosition(ImVec2 popupSize) {
        ImVec2 halfWinSize(windowSize.x / 2.0f, windowSize.y / 2.0f);
        ImVec2 halfPopupSize = popupSize / 2.0f;
        return ImGui::GetMainViewport()->Pos + (halfWinSize - halfPopupSize);
    }

    void saveFileModal(const char* title, std::function<void(const char*)> saveCallback) {
        ImVec2 popupSize(windowSize.x - 50.0f, windowSize.y - 50.0f);
        ImGui::SetNextWindowPos(getPopupPosition(popupSize));
        ImGui::SetNextWindowSize(popupSize);

        if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
            std::string* currentFile = (std::string*)ImGui::GetStateStorage()->GetVoidPtr(ImGui::GetID("file"));
            std::string* currentDirectory = (std::string*)ImGui::GetStateStorage()->GetVoidPtr(ImGui::GetID("savedir"));

            if (currentFile == nullptr) {
                currentFile = new std::string;
                ImGui::GetStateStorage()->SetVoidPtr(ImGui::GetID("file"), currentFile);
            }

            if (currentDirectory == nullptr) {
                currentDirectory = new std::string;
                ImGui::GetStateStorage()->SetVoidPtr(ImGui::GetID("savedir"), currentDirectory);
            }

            ImGui::Text("%s", currentDirectory->c_str());

            ImGui::BeginChild("Stuffs", ImVec2(ImGui::GetWindowWidth() - 17.0f, ImGui::GetWindowHeight() - 90.0f), true);

            listDirectoryItems(*currentDirectory,
                [&](std::string path, PHYSFS_FileType type) {
                    if (type == PHYSFS_FILETYPE_DIRECTORY) *currentDirectory = path;
                    else *currentFile = std::filesystem::path(path).filename().string();
                },
                [&](std::string path, PHYSFS_FileType type) {
                    if (type == PHYSFS_FILETYPE_DIRECTORY) *currentDirectory = path;
                    else {
                        *currentFile = std::filesystem::path(path).filename().string();
                        saveCallback((*currentDirectory + "/" + *currentFile).c_str());
                    }
                });


            ImGui::EndChild();

            ImGui::PushItemWidth(ImGui::GetWindowWidth() - 109.0f);
            ImGui::InputText("", currentFile);

            std::string fullPath = *currentDirectory + "/" + *currentFile;

            ImGui::SameLine();
            if (ImGui::Button("OK") && !fullPath.empty()) {
                logMsg("fullPath: %s", fullPath.c_str());
                saveCallback(fullPath.c_str());
                ImGui::GetStateStorage()->Clear();
                ImGui::CloseCurrentPopup();
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
        openFileModalOffset(title, openCallback, "", fileExtensions, fileExtensionCount, startingDir);
    }

    void openFileModalOffset(const char* title, std::function<void(const char*)> openCallback, const char* rootOffset, const char** fileExtensions, int fileExtensionCount, const char* startingDir) {
        ImVec2 popupSize(windowSize.x - 50.0f, windowSize.y - 50.0f);
        ImGui::SetNextWindowPos(getPopupPosition(popupSize));
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
            char** files = PHYSFS_enumerateFiles((std::string(rootOffset) + *currentDirectory).c_str());

            std::string titleDoesntExist = "File Doesn't Exist##";
            titleDoesntExist += title;

            if (!currentDirectory->empty()) {
                ImGui::Text("..");
                if (ImGui::IsItemClicked()) {
                    std::filesystem::path p(*currentDirectory);
                    *currentDirectory = p.parent_path().string();
                }
            }

            std::vector<char*> fileVec;
            for (char** currFile = files; *currFile != nullptr; currFile++) {
                fileVec.push_back(*currFile);
            }

            std::sort(fileVec.begin(), fileVec.end(), [&](const char* pathA, const char* pathB) {
                std::string absPathA = currentDirectory->empty() ? pathA : (*currentDirectory + "/" + pathA);
                PHYSFS_Stat statA;
                PHYSFS_stat(absPathA.c_str(), &statA);
                std::string extensionA = std::filesystem::path(pathA).extension().string();

                std::string absPathB = currentDirectory->empty() ? pathB : (*currentDirectory + "/" + pathB);
                PHYSFS_Stat statB;
                PHYSFS_stat(absPathB.c_str(), &statB);
                std::string extensionB = std::filesystem::path(pathB).extension().string();

                if (extensionA != extensionB)
                    return extensionA < extensionB;
                else
                    return absPathA < absPathB;
                });

            for (auto currFile : fileVec) {
                std::string absPath = rootOffset + (currentDirectory->empty() ? currFile : (*currentDirectory + "/" + currFile));
                PHYSFS_Stat stat;
                PHYSFS_stat(absPath.c_str(), &stat);
                std::string extension = std::filesystem::path(currFile).extension().string();

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

                ImGui::Text("%s%s%s", icon, currFile, stat.filetype == PHYSFS_FILETYPE_DIRECTORY ? "/" : "");

                if (ImGui::IsItemClicked()) {
                    if (stat.filetype == PHYSFS_FILETYPE_REGULAR) {
                        *fullFilePath = absPath;
                        logMsg("selected %s", fullFilePath->c_str());
                    } else {
                        if (*currentDirectory != "/")
                            *currentDirectory += "/";
                        *currentDirectory += currFile;

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

            ImGui::PushItemWidth(ImGui::GetWindowWidth() - 115.0f);
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

    void openFileFullFSModal(const char* title, std::function<void(const char*)> openCallback) {
        ImVec2 popupSize(windowSize.x - 50.0f, windowSize.y - 50.0f);
        ImGui::SetNextWindowPos(getPopupPosition(popupSize));
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
#ifdef _WIN32
                *currentDirectory = "C:\\";
#else
                *currentDirectory = "/";
#endif
                ImGui::GetStateStorage()->SetVoidPtr(ImGui::GetID("savedir"), currentDirectory);
            }

            ImGui::Text("%s", currentDirectory->c_str());

            ImGui::BeginChild("Stuffs", ImVec2(ImGui::GetWindowWidth() - 17.0f, ImGui::GetWindowHeight() - 90.0f), true);

            std::string titleDoesntExist = "File Doesn't Exist##";
            titleDoesntExist += title;

            if (!currentDirectory->empty()) {
                ImGui::Text("..");
                if (ImGui::IsItemClicked()) {
                    std::string oldDir = *currentDirectory;
                    std::filesystem::path p(*currentDirectory);
                    *currentDirectory = p.parent_path().string();

                    if (*currentDirectory == oldDir) {
                        currentDirectory->clear();
                    }
                } else {
                    std::vector<std::filesystem::directory_entry> fileVec;
                    std::filesystem::path currentPath{ *currentDirectory };

                    for (auto& entry : std::filesystem::directory_iterator{ currentPath }) {
                        fileVec.push_back(entry);
                    }

                    std::sort(fileVec.begin(), fileVec.end(), [&](const std::filesystem::directory_entry& entryA, const std::filesystem::directory_entry& entryB) {
                        if (entryB.is_directory() && !entryA.is_directory())
                            return false;
                        else if (entryA.is_directory() && !entryB.is_directory())
                            return true;

                        std::string extensionA = entryA.path().extension().string();

                        std::string extensionB = entryB.path().extension().string();

                        if (extensionA != extensionB)
                            return extensionA < extensionB;
                        else
                            return entryA.path() < entryB.path();
                        });

                    for (auto& currFile : fileVec) {
                        std::string absPath = currFile.path().string();
                        std::string extension = currFile.path().extension().string();
                        std::string filename = currFile.path().filename().string();

#ifdef _WIN32
                        if (slib::Win32Util::isFileHidden(absPath.c_str()))
                            continue;
#endif

                        const char* icon = "  ";

                        if (currFile.is_directory()) {
                            icon = (const char*)(ICON_FA_FOLDER u8" ");
                        } else icon = getIcon(extension);

                        ImGui::Text("%s%s%s", icon, filename.c_str(), currFile.is_directory() ? "/" : "");

                        if (ImGui::IsItemClicked()) {
                            if (currFile.is_regular_file()) {
                                *fullFilePath = absPath;
                                logMsg("selected %s", fullFilePath->c_str());
                            } else {
                                *currentDirectory = currFile.path().string();

                                logMsg("navigated to %s", currentDirectory->c_str());
                            }
                        }

                        if (ImGui::IsItemHovered() && currFile.is_regular_file()) {
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
                }
            } else {
#ifdef _WIN32
                slib::Win32Util::enumerateDriveLetters([](char letter, void* dirPtr) {
                    std::string* currentDirectory = (std::string*)dirPtr;

                    ImGui::Text("%c", letter);

                    if (ImGui::IsItemClicked()) {
                        *currentDirectory = letter;
                        *currentDirectory += ":\\";
                    }
                    }, currentDirectory);
#else
                *currentDirectory = "/";
#endif
            }

            ImGui::EndChild();

            ImGui::PushItemWidth(ImGui::GetWindowWidth() - 115.0f);
            ImGui::InputText("", fullFilePath);

            ImGui::SameLine();
            if (ImGui::Button("OK") && !fullFilePath->empty()) {
                if (!std::filesystem::exists(*fullFilePath)) {
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

    struct MessageBox {
        const char* title;
        const char* desc;
        std::function<void(bool)> callback;
        MessageBoxType type;
        bool opened = false;

        void destroy() {
            free((void*)title);
            free((void*)desc);
        }
    };

    std::vector<MessageBox> messageBoxes;

    void messageBoxModal(const char* title, const char* desc, std::function<void(bool)> callback, MessageBoxType type) {
        MessageBox mbox{
            .title = strdup(title),
            .desc = strdup(desc),
            .callback = callback,
            .type = type
        };

        messageBoxes.push_back(std::move(mbox));
    }

    void drawModals() {
        if (messageBoxes.size() == 0) return;
        MessageBox& mbox = messageBoxes.back();

        ImVec2 popupSize(500.0f, 150.0f);
        ImVec2 popupCorner = (ImVec2(windowSize) * 0.5f) - (popupSize * 0.5f);
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->Pos + popupCorner);
        ImGui::SetNextWindowSize(popupSize);

        if (ImGui::BeginPopupModal(mbox.title, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
            ImGui::TextWrapped("%s", mbox.desc);

            switch (mbox.type) {
            case MessageBoxType::YesNo: {
                if (ImGui::Button("Yes")) {
                    if (mbox.callback) mbox.callback(true);
                    ImGui::CloseCurrentPopup();
                    mbox.destroy();
                    messageBoxes.pop_back();
                }

                ImGui::SameLine();

                if (ImGui::Button("No")) {
                    if (mbox.callback) mbox.callback(false);
                    ImGui::CloseCurrentPopup();
                    mbox.destroy();
                    messageBoxes.pop_back();
                }
                break;
            }
            case MessageBoxType::Ok: {
                if (ImGui::Button("Ok")) {
                    if (mbox.callback) mbox.callback(true);
                    ImGui::CloseCurrentPopup();
                    mbox.destroy();
                    messageBoxes.pop_back();
                }
                break;
            }
            }
            ImGui::EndPopup();
        }

        if (!mbox.opened) {
            ImGui::OpenPopup(mbox.title);
        }
    }

    struct PopupNotification {
        std::string text;
        float shownFor = 0.0f;
        NotificationType type;
    };

    std::vector<PopupNotification> popupNotifications;

    ImColor getNotificationTextColor(NotificationType type) {
        switch (type) {
        default:
        case NotificationType::Info:
            return ImColor(255, 255, 255);
        case NotificationType::Warning:
            return ImColor(255, 255, 0);
        case NotificationType::Error:
            return ImColor(255, 0, 0);
        }
    }

    ImColor getNotificationBorderColor(NotificationType type) {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;
        switch (type) {
        default:
        case NotificationType::Info:
            return colors[ImGuiCol_Border];
        case NotificationType::Warning:
            return ImColor(255, 255, 0);
        case NotificationType::Error:
            return ImColor(255, 0, 0);
        }
    }

    void addNotification(std::string text, NotificationType type) {
        popupNotifications.push_back(PopupNotification{ .text = text, .type = type });
    }

    void drawPopupNotifications() {
        ImDrawList* drawList = ImGui::GetForegroundDrawList();

        ImVec2 popupSize(300, 45);

        ImVec2 popupCorner = ImVec2(windowSize) - popupSize - ImVec2(15, 15);
        ImGuiStyle& style = ImGui::GetStyle();
        const ImColor popupBg = style.Colors[ImGuiCol_WindowBg];
        const float popupDuration = 5.0f;

        for (auto it = popupNotifications.rbegin(); it != popupNotifications.rend(); it++) {
            PopupNotification& notification = *it;
            ImVec2 thisPopupSize = popupSize;
            ImVec2 textSize = ImGui::CalcTextSize(notification.text.c_str());
            thisPopupSize.x = glm::max(textSize.x, thisPopupSize.x);

            float alpha = glm::min(glm::min(1.0f, notification.shownFor * 5.0f), ((popupDuration - 0.5f) - notification.shownFor) * 2.0f);

            ImColor animatedBg = popupBg;
            ImColor animatedBorder = getNotificationBorderColor(notification.type);
            ImColor animatedTextColor = getNotificationTextColor(notification.type);

            animatedTextColor.Value.w = alpha;
            animatedBg.Value.w = alpha * 0.8f;
            animatedBorder.Value.w = alpha;

            ImVec2 animatedOffset(glm::max(glm::pow((0.5f - notification.shownFor) * 2.0f, 3.0f), 0.0f) * 150.0f, 0.0f);

            ImVec2 min = popupCorner + animatedOffset;
            ImVec2 max = popupCorner + animatedOffset + popupSize;

            drawList->AddRectFilled(min, max, animatedBg, 7.0f);
            drawList->AddRect(min, max, animatedBorder, 7.0f, ~0, 2.0f);
            ImVec2 textPos = ImVec2(min.x + 4.0f, min.y + popupSize.y * 0.25f);
            drawList->AddText(textPos, animatedTextColor, notification.text.c_str());

            notification.shownFor += ImGui::GetIO().DeltaTime;
            popupCorner.y -= 55.0f;
        }

        popupNotifications.erase(std::remove_if(popupNotifications.begin(), popupNotifications.end(),
            [popupDuration](PopupNotification& notification) { return notification.shownFor > popupDuration; }), popupNotifications.end());
    }

    bool selectAssetPopup(const char* title, AssetID& id, bool open, bool source) {
        static std::string path;
        bool changed = false;

        std::filesystem::path p(path);
        p = p.parent_path();

        openFileModalOffset(title, [&](const char* path) {
            id = AssetDB::pathToId(path);
            changed = true;
            }, source ? "SourceData/" : "", nullptr, 0, p.string().c_str());

        if (open) {
            if (id != ~0u)
                path = AssetDB::idToPath(id);
            else
                path = "";
            ImGui::OpenPopup(title);
        }

        return changed;
    }
    
    bool selectRawAssetPopup(const char* title, AssetID& id, bool open) {
        static std::string path;
        bool changed = false;

        std::filesystem::path p(path);
        p = p.parent_path();

        if (path.starts_with("Raw/"))
            p = p.string().substr(4);

        openFileModalOffset(title, [&](const char* path) {
            id = AssetDB::pathToId(path);
            changed = true;
            }, "Raw/", nullptr, 0, p.string().c_str());

        if (open) {
            if (id != ~0u)
                path = AssetDB::idToPath(id);
            else
                path = "";
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

    void pushBoldFont() {
        ImGui::PushFont(boldFont);
    }

    namespace EditorUI {
        void centeredText(const char *text) {
            float windowWidth = ImGui::GetWindowSize().x;
            float textWidth   = ImGui::CalcTextSize(text).x;

            ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
            ImGui::TextUnformatted(text);
        }
    }
}
