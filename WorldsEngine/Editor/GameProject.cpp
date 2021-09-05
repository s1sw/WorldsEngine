#include "Editor.hpp"
#include <fstream>
#include <nlohmann/json.hpp>
#include <slib/Path.hpp>
#include <Core/Log.hpp>
#include <filesystem>


namespace worlds {
    GameProject::GameProject(std::string path) {
        // Parse the project file from the specified path
        std::ifstream i(path);
        nlohmann::json j;
        i >> j;

        _name = j["projectName"];

        slib::Path parsedPath{ path.c_str() };
        slib::Path rootPath = parsedPath.parentPath();

        _root = rootPath.cStr();

        _srcDataPath = ((slib::String)rootPath + "/SourceData").cStr();
        _compiledDataPath = ((slib::String)rootPath + "/Data").cStr();
        _rawPath = ((slib::String)rootPath + "/Raw").cStr();

        for (auto& dir : j["copyDirectories"]) {
            _copyDirs.push_back(dir);
        }
    }

    std::string_view GameProject::name() const {
        return _name;
    }

    std::string_view GameProject::root() const {
        return _root;
    }

    void GameProject::mountPaths() {
        logMsg("Mounting %s as compiled data path", _compiledDataPath.c_str());
        logMsg("Mounting %s as source data path", _srcDataPath.c_str());
        PHYSFS_mount(_compiledDataPath.c_str(), "/", 0);
        PHYSFS_mount(_srcDataPath.c_str(), "/SourceData", 0);
        PHYSFS_mount(_rawPath.c_str(), "/Raw", 0);
        PHYSFS_mount((_root + "/Temp").c_str(), "/Temp", 0);
        PHYSFS_setWriteDir(_root.c_str());

        for (const std::string& dir : _copyDirs) {
            std::string dirPath = _srcDataPath + "/" + dir;
            logMsg("Mouting %s as %s", dirPath.c_str(), dir.c_str());
            if (PHYSFS_mount(dirPath.c_str(), dir.c_str(), 1) == 0) {
                logErr("Error mounting: %s", PHYSFS_getLastError());
            }
        }
    }

    void GameProject::unmountPaths() {
        PHYSFS_unmount(_compiledDataPath.c_str());
        PHYSFS_unmount(_srcDataPath.c_str());
        PHYSFS_unmount(_rawPath.c_str());
        PHYSFS_unmount((_root + "/Temp").c_str());

        for (const std::string& dir : _copyDirs) {
            std::string dirPath = _srcDataPath + "/" + dir;
            if (PHYSFS_unmount(dirPath.c_str()) == 0) {
                logErr("Error unmounting: %s", PHYSFS_getLastError());
            }
        }
    }
}