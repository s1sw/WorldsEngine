#pragma once
#include "../Util/Result.hpp"
#include <string>

namespace worlds
{
    typedef uint32_t AssetID;
    enum class IOError
    {
        None,
        FileNotFound,
        CouldntStat,
        IncompleteRead,
        Unknown
    };

    inline const char* getIOErrorStr(IOError err)
    {
        switch (err)
        {
        case IOError::None:
            return "none";
        case IOError::FileNotFound:
            return "not found";
        case IOError::IncompleteRead:
            return "incomplete read";
        case IOError::Unknown:
            return "unknown";
        case IOError::CouldntStat:
            return "couldn't stat";
        }
        return "";
    }

    Result<void*, IOError> LoadFileToBuffer(std::string path, int64_t* fileLength);
    Result<std::string, IOError> LoadFileToString(std::string path);
    Result<void*, IOError> loadAssetToBuffer(AssetID id, int64_t* fileLength);
    bool canOpenFile(std::string path);
}
