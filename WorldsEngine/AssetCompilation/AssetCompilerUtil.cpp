#include "AssetCompilerUtil.hpp"

namespace worlds {
    std::string getOutputPath(std::string srcPath, bool replaceExtension) {
        // Remove SrcData/
        const std::string srcRoot = "SrcData/";
        srcPath = srcPath.substr(srcRoot.size());
        if (replaceExtension) {
            // If the file extension ends in j (meaing it's a source j file), remove the suffix
            srcPath = srcPath.substr(0, srcPath.size() - 1);
        }

        return srcPath;
    }
}
