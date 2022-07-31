#include <Render/MaterialManager.hpp>
#include <Core/AssetDB.hpp>
#include <Core/Log.hpp>

namespace worlds
{
    nlohmann::json& MaterialManager::loadOrGet(AssetID id)
    {
        if (mats.contains(id))
        {
            return mats.at(id);
        }

        PHYSFS_File* f = AssetDB::openAssetFileRead(id);

        if (f == nullptr)
        {
            std::string path = AssetDB::idToPath(id);
            auto err = PHYSFS_getLastErrorCode();
            auto errStr = PHYSFS_getErrorByCode(err);
            logErr(WELogCategoryRender, "Failed to open %s: %s", path.c_str(), errStr);
            f = PHYSFS_openRead("Materials/missing.json");
        }

        size_t fileSize = PHYSFS_fileLength(f);
        std::string str;
        str.resize(fileSize);
        // Add a null byte to the end to make a C string
        PHYSFS_readBytes(f, str.data(), fileSize);
        PHYSFS_close(f);

        try
        {
            auto j = nlohmann::json::parse(str);

            if (j.type() != nlohmann::detail::value_t::object)
            {
                logErr(WELogCategoryRender, "Invalid material document");
            }

            mats.insert({ id, std::move(j) });
        }
        catch(nlohmann::detail::exception& ex)
        {
            std::string path = AssetDB::idToPath(id);
            logErr(WELogCategoryRender, "Invalid material document %s (%s)", path.c_str(), ex.what());
        }

        return mats.at(id);
    }
}