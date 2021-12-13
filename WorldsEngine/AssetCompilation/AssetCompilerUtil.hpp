#pragma once
#include <string>

namespace worlds {
    typedef uint32_t AssetID;
    std::string getOutputPath(std::string srcPath, bool replaceExtension = true);
    AssetID getOutputAsset(std::string srcPath, bool replaceExtension = true);
}
