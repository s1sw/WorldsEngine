#include "CubemapLoader.hpp"
#include "tracy/Tracy.hpp"
#include <Core/Engine.hpp>
#include <Core/JobSystem.hpp>
#include <Core/Log.hpp>
#include <Util/TimingUtil.hpp>
#include <algorithm>
#define CRND_HEADER_FILE_ONLY
#include <crn_decomp.h>
#include <nlohmann/json.hpp>

namespace worlds
{
    CubemapData loadCubemapData(AssetID asset)
    {
        ZoneScoped;
        PerfTimer timer;

        PHYSFS_File *f = AssetDB::openAssetFileRead(asset);
        size_t fileSize = PHYSFS_fileLength(f);
        std::string str;
        str.resize(fileSize);
        PHYSFS_readBytes(f, str.data(), fileSize);
        PHYSFS_close(f);

        nlohmann::json cubemapDoc = nlohmann::json::parse(str);

        if (!cubemapDoc.is_array())
        {
            logErr(WELogCategoryRender, "Invalid cubemap document");
            return CubemapData{};
        }

        if (cubemapDoc.size() != 6)
        {
            logErr(WELogCategoryRender, "Invalid cubemap document");
            return CubemapData{};
        }

        CubemapData cd;
        JobList &jl = g_jobSys->getFreeJobList();
        jl.begin();
        for (int i = 0; i < 6; i++)
        {
            Job j{[i, &cd, &cubemapDoc] {
                auto val = cubemapDoc[i];
                cd.faceData[i] = loadTexData(AssetDB::pathToId(val.get<std::string>()));
            }};
            jl.addJob(std::move(j));
        }
        jl.end();
        g_jobSys->signalJobListAvailable();
        jl.wait();

        cd.debugName = AssetDB::idToPath(asset);

        logVrb("Spent %.3fms loading cubemap %s", timer.stopGetMs(), cd.debugName.c_str());
        return cd;
    }
}
