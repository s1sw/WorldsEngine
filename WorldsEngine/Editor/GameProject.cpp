#include "Editor.hpp"
#include <Core/Log.hpp>
#include <Editor/ProjectAssetCompiler.hpp>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <physfs.h>
#include <slib/Path.hpp>

namespace worlds
{
    GameProject::GameProject(std::string path)
    {
        // Parse the project file from the specified path
        std::ifstream i(path);
        nlohmann::json j;
        i >> j;

        _name = j["projectName"];

        slib::Path parsedPath{path.c_str()};
        slib::Path rootPath = parsedPath.parentPath();

        _root = rootPath.cStr();

        _srcDataPath = ((slib::String)rootPath + "/SourceData").cStr();
        _compiledDataPath = ((slib::String)rootPath + "/Data").cStr();
        _rawPath = ((slib::String)rootPath + "/Raw").cStr();

        for (auto &dir : j["copyDirectories"])
        {
            _copyDirs.push_back(dir);
        }

        slib::String tempDir = ((slib::String)rootPath + "/Temp");
        bool tempDirExists = std::filesystem::is_directory(tempDir.cStr());

        if (!tempDirExists)
        {
            std::filesystem::create_directory(tempDir.cStr());
            logVrb("Creating project temp directory %s", tempDir.cStr());
        }

        _projectAssets = std::make_unique<ProjectAssets>(*this);
        _assetCompiler = std::make_unique<ProjectAssetCompiler>(*this);
    }

    std::string_view GameProject::name() const
    {
        return _name;
    }

    std::string_view GameProject::root() const
    {
        return _root;
    }

    std::string_view GameProject::builtData() const
    {
        return _compiledDataPath;
    }

    std::string_view GameProject::rawData() const
    {
        return _rawPath;
    }

    std::string_view GameProject::sourceData() const
    {
        return _srcDataPath;
    }

    ProjectAssets &GameProject::assets()
    {
        return *_projectAssets;
    }

    ProjectAssetCompiler &GameProject::assetCompiler()
    {
        return *_assetCompiler;
    }

    void GameProject::mountPaths()
    {
        logMsg("Mounting project %s", _name.c_str());
        logVrb("Mounting %s as compiled data path", _compiledDataPath.c_str());
        logVrb("Mounting %s as source data path", _srcDataPath.c_str());
        PHYSFS_mount(_compiledDataPath.c_str(), "/", 0);
        PHYSFS_mount(_srcDataPath.c_str(), "/SourceData", 0);
        PHYSFS_mount(_rawPath.c_str(), "/Raw", 0);
        PHYSFS_mount((_root + "/Temp").c_str(), "/Temp", 0);
        PHYSFS_setWriteDir(_root.c_str());

        for (const std::string &dir : _copyDirs)
        {
            std::string dirPath = _srcDataPath + "/" + dir;
            logVrb("Mounting %s as %s", dirPath.c_str(), dir.c_str());
            if (PHYSFS_mount(dirPath.c_str(), dir.c_str(), 1) == 0)
            {
                PHYSFS_ErrorCode errCode = PHYSFS_getLastErrorCode();
                logErr("Error mounting %s: %s", dirPath.c_str(), PHYSFS_getErrorByCode(errCode));
            }
        }

        _projectAssets->enumerateAssets();
    }

    void GameProject::unmountPaths()
    {
        PHYSFS_unmount(_compiledDataPath.c_str());
        PHYSFS_unmount(_srcDataPath.c_str());
        PHYSFS_unmount(_rawPath.c_str());
        PHYSFS_unmount((_root + "/Temp").c_str());

        for (const std::string &dir : _copyDirs)
        {
            std::string dirPath = _srcDataPath + "/" + dir;
            if (PHYSFS_unmount(dirPath.c_str()) == 0)
            {
                PHYSFS_ErrorCode errCode = PHYSFS_getLastErrorCode();
                logErr("Error unmounting %s: %s", dirPath.c_str(), PHYSFS_getErrorByCode(errCode));
            }
        }
    }
}
