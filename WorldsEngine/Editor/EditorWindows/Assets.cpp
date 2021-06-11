#include "EditorWindows.hpp"
#include "../../ImGui/imgui.h"
#include <filesystem>
#include "../../Render/Loaders/SourceModelLoader.hpp"
#include "../../Util/CreateModelObject.hpp"
#include "../../Core/Console.hpp"
#include "../../Libs/IconsFontAwesome5.h"
#include "../GuiUtil.hpp"
#include <slib/Path.hpp>

namespace worlds {
    void Assets::draw(entt::registry& reg) {
        static std::string currentDir = "";

        static ConVar showExts{"editor_assetExtDbg", "0", "Shows parsed file extensions in brackets."};
        if (ImGui::Begin(ICON_FA_FOLDER u8" Assets", &active)) {
            if (ImGui::Button("..")) {
                std::filesystem::path p{ currentDir };
                currentDir = p.parent_path().string();
                if (currentDir == "/")
                    currentDir = "";

                if (currentDir[0] == '/') {
                    currentDir = currentDir.substr(1);
                }
                logMsg("Navigated to %s", currentDir.c_str());
            }

            char** files = PHYSFS_enumerateFiles(currentDir.c_str());

            for (char** currFile = files; *currFile != nullptr; currFile++) {
                slib::Path path{*currFile};
                std::string origDirStr = currentDir;
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
                    slib::String buttonLabel {(const char*)ICON_FA_FOLDER};
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
                    slib::Path p{fullPath.c_str()};
                    auto ext = p.fileExtension();
                    const char* icon = getIcon(ext.cStr());
                    slib::String buttonLabel = icon;
                    buttonLabel += *currFile;

                    ImGui::Text("%s", buttonLabel.cStr());

                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
                        ImGui::IsItemHovered()) {
                        editor->currentSelectedAsset = AssetDB::pathToId(fullPath);
                    }
                }
            }

            PHYSFS_freeList(files);
        }

        ImGui::End();
    }
}
