#include "TextureCompiler.hpp"
#include "AssetCompilerUtil.hpp"
#include "../Libs/crnlib/crn_texture_conversion.h"
#include "Core/Log.hpp"
#include "../Render/Loaders/TextureLoader.hpp"
#include <nlohmann/json.hpp>
#include "../IO/IOUtil.hpp"
using namespace crnlib;

namespace worlds {
    TextureCompiler::TextureCompiler() {
        AssetCompilers::registerCompiler(this);
    }

    texture_type getTextureType(std::string typeStr) {
        if (typeStr == "regular") {
            return cTextureTypeRegularMap;
        } else if (typeStr == "normal") {
            return cTextureTypeNormalMap;
        } else {
            logErr("Invalid texture type: %s", typeStr.c_str());
            return cTextureTypeRegularMap;
        }
    }

    AssetID TextureCompiler::compile(AssetID src) {
        std::string inputPath = AssetDB::idToPath(src);
        auto jsonContents = LoadFileToString(inputPath);

        if (jsonContents.error != IOError::None) {
            logErr("Error opening asset file");
            return INVALID_ASSET;
        }

        nlohmann::json j = nlohmann::json::parse(jsonContents.value);

        std::string outputPath = getOutputPath(AssetDB::idToPath(src));
        logMsg("Compiled %s to %s", AssetDB::idToPath(src).c_str(), outputPath.c_str());

        TextureData inTexData = loadTexData(AssetDB::pathToId(j["srcPath"].get<std::string>()));

        crn_comp_params compParams;
        compParams.m_width = inTexData.width;
        compParams.m_height = inTexData.height;
        compParams.set_flag(cCRNCompFlagPerceptual, true);
        compParams.m_file_type = cCRNFileTypeCRN;
        compParams.m_format = cCRNFmtDXT5;
        compParams.m_pImages[0][0] = (crn_uint32*)inTexData.data;
        compParams.m_quality_level = 127;
        compParams.m_num_helper_threads = 8;

        crn_mipmap_params mipParams;
        mipParams.m_gamma_filtering = true;
        mipParams.m_mode = cCRNMipModeGenerateMips;

        crn_uint32 outputSize;
        float actualBitrate;
        crn_uint32 actualQualityLevel;

        void* outData = crn_compress(compParams, mipParams, outputSize, &actualQualityLevel, &actualBitrate);

        PHYSFS_File* outFile = PHYSFS_openWrite(outputPath.c_str());
        PHYSFS_writeBytes(outFile, outData, outputSize);
        PHYSFS_close(outFile);

        crn_free_block(outData);

        delete inTexData.data;

        return AssetDB::pathToId(outputPath);
    }

    const char* TextureCompiler::getSourceExtension() {
        return ".wtexj";
    }

    const char* TextureCompiler::getCompiledExtension() {
        return ".wtex";
    }
}
