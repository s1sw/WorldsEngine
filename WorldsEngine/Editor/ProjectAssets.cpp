#include "Editor.hpp"
#include "fts_fuzzy_match.h"
#include <AssetCompilation/AssetCompilerUtil.hpp>
#include <AssetCompilation/AssetCompilers.hpp>
#include <Core/AssetDB.hpp>
#include <Core/Log.hpp>
#include <filesystem>
#include <physfs.h>

namespace worlds
{
    ProjectAssets::ProjectAssets(const GameProject &project) : project(project)
    {
    }

    void ProjectAssets::checkForAssetChanges()
    {
        for (AssetFile &af : assetFiles)
        {
            checkForAssetChange(af);
        }
    }

    void ProjectAssets::checkForAssetChange(AssetFile &file)
    {
        if (!PHYSFS_exists(file.compiledPath.c_str()))
        {
            file.needsCompile = true;
            return;
        }

        if (!file.isCompiled)
            return;

        PHYSFS_Stat compiledStat;
        PHYSFS_stat(file.compiledPath.c_str(), &compiledStat);

        if (compiledStat.modtime < file.lastModified)
            file.needsCompile = true;

        IAssetCompiler *compiler = AssetCompilers::getCompilerFor(file.sourceAssetId);
        std::vector<std::string> dependencies;
        compiler->getFileDependencies(file.sourceAssetId, dependencies);

        file.dependenciesExist = true;
        for (auto &dependency : dependencies)
        {
            if (!PHYSFS_exists(dependency.c_str()))
            {
                file.dependenciesExist = false;
                continue;
            }

            PHYSFS_Stat dependencyStat;
            PHYSFS_stat(dependency.c_str(), &dependencyStat);

            if (dependencyStat.modtime > compiledStat.modtime)
                file.needsCompile = true;
        }
    }

    void ProjectAssets::enumerateAssets()
    {
        assetFiles.clear();
        enumerateForAssets("SourceData");
    }

    void ProjectAssets::enumerateForAssets(const char *path)
    {
        char **list = PHYSFS_enumerateFiles(path);

        for (char **p = list; *p != nullptr; p++)
        {
            char *subpath = *p;
            std::string fullPath = std::string(path) + '/' + subpath;

            PHYSFS_Stat stat;
            PHYSFS_stat(fullPath.c_str(), &stat);

            if (stat.filetype == PHYSFS_FILETYPE_DIRECTORY)
            {
                enumerateForAssets(fullPath.c_str());
            }
            else if (stat.filetype == PHYSFS_FILETYPE_REGULAR)
            {
                std::filesystem::path sp = subpath;
                if (sp.has_extension())
                {
                    std::string extension = std::filesystem::path{subpath}.extension().string();

                    if (extension == ".wmdlj" || extension == ".wtexj")
                    {
                        assetFiles.push_back(AssetFile{.sourceAssetId = AssetDB::pathToId(fullPath),
                                                       .path = fullPath,
                                                       .lastModified = stat.modtime,
                                                       .compiledPath = getOutputPath(fullPath).substr(5),
                                                       .isCompiled = true});
                    }
                    else if (extension == ".wscn" || extension == ".wprefab")
                    {
                        assetFiles.push_back(AssetFile{.sourceAssetId = AssetDB::pathToId(fullPath),
                                                       .path = fullPath,
                                                       .lastModified = stat.modtime,
                                                       .isCompiled = false});
                    }
                }
            }
        }

        PHYSFS_freeList(list);
    }

    struct AssetSearchCandidate
    {
        AssetID assetId;
        int score;
    };

    slib::List<AssetID> ProjectAssets::searchForAssets(slib::String pattern)
    {
        std::vector<AssetSearchCandidate> candidates;

        for (AssetFile &af : assetFiles)
        {
            int score;
            if (fts::fuzzy_match(pattern.cStr(), af.path.c_str(), score))
            {
                candidates.push_back({af.sourceAssetId, score});
            }
        }

        std::sort(candidates.begin(), candidates.end(),
                  [](const AssetSearchCandidate &a, const AssetSearchCandidate &b) { return a.score > b.score; });

        slib::List<AssetID> orderedIds;
        orderedIds.reserve(candidates.size());

        for (AssetSearchCandidate &sc : candidates)
        {
            orderedIds.add(sc.assetId);
        }

        return orderedIds;
    }
}