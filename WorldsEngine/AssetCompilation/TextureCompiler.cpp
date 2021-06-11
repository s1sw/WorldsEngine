#include "TextureCompiler.hpp"
#include "AssetCompilerUtil.hpp"
#include "../Libs/crnlib/crn_texture_conversion.h"
#include "Core/Log.hpp"
#include "../Render/Loaders/TextureLoader.hpp"
#include <nlohmann/json.hpp>
#include "../IO/IOUtil.hpp"
#include <filesystem>
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

    struct TexCompileThreadInfo {
        nlohmann::json j;
        std::string inputPath;
        std::string outputPath;
        AssetCompileOperation* compileOp;
    };

    AssetCompileOperation* TextureCompiler::compile(AssetID src) {
        std::string inputPath = AssetDB::idToPath(src);
        std::string outputPath = getOutputPath(inputPath);

        auto jsonContents = LoadFileToString(inputPath);

        if (jsonContents.error != IOError::None) {
            logErr("Error opening asset file");
            return nullptr;
        }

        AssetCompileOperation* compileOp = new AssetCompileOperation;
        compileOp->outputId = AssetDB::pathToId(outputPath);
        TexCompileThreadInfo* tcti = new TexCompileThreadInfo;
        tcti->j = nlohmann::json::parse(jsonContents.value);
        tcti->inputPath = inputPath;
        tcti->outputPath = outputPath;
        tcti->compileOp = compileOp;

        std::thread([tcti, this]() {
            compileInternal(tcti->j, tcti->inputPath, tcti->outputPath, tcti->compileOp);
            delete tcti;
        }).detach();

        logMsg("Compiling %s to %s", AssetDB::idToPath(src).c_str(), outputPath.c_str());
        return compileOp;
    }

    const char* TextureCompiler::getSourceExtension() {
        return ".wtexj";
    }

    const char* TextureCompiler::getCompiledExtension() {
        return ".wtex";
    }

    crn_bool progressCallback(crn_uint32 phaseIndex, crn_uint32 totalPhases, crn_uint32 subphaseIndex, crn_uint32 totalSubphases, void* data) {
        AssetCompileOperation* compileOp = (AssetCompileOperation*)data;
        float phaseProgress = 1.0f / totalPhases;
        compileOp->progress = (phaseProgress * phaseIndex) + ((phaseProgress / totalSubphases) * subphaseIndex);
        return true;
    }

    void TextureCompiler::compileInternal(nlohmann::json j, std::string inputPath, std::string outputPath, AssetCompileOperation* compileOp) {
        bool isSrgb = j.value("isSrgb", true);
        TextureData inTexData = loadTexData(AssetDB::pathToId(j["srcPath"].get<std::string>()));
        logMsg("Texture is %ix%i", inTexData.width, inTexData.height);

        crn_comp_params compParams;
        compParams.m_width = inTexData.width;
        compParams.m_height = inTexData.height;
        compParams.set_flag(cCRNCompFlagPerceptual, isSrgb);
        compParams.m_file_type = cCRNFileTypeCRN;
        compParams.m_format = j["type"] == "regular" ? cCRNFmtDXT5 : cCRNFmtDXN_XY;
        compParams.m_pImages[0][0] = (crn_uint32*)inTexData.data;
        compParams.m_quality_level = 127;
        compParams.m_num_helper_threads = 4;
        compParams.m_pProgress_func = progressCallback;
        compParams.m_pProgress_func_data = compileOp;
        logMsg("perceptual: %i, format: %i", compParams.get_flag(cCRNCompFlagPerceptual), compParams.m_format);

        crn_mipmap_params mipParams;
        mipParams.m_gamma_filtering = true;
        mipParams.m_mode = cCRNMipModeGenerateMips;

        crn_uint32 outputSize;
        float actualBitrate;
        crn_uint32 actualQualityLevel;

        void* outData = crn_compress(compParams, mipParams, outputSize, &actualQualityLevel, &actualBitrate);
        logMsg("Compressed %s with actual quality of %u and bitrate of %f", inputPath.c_str(), actualQualityLevel, actualBitrate);

        // TODO: Assumes that we are writing to the GameData directory! BADDD!!!!
        std::filesystem::path fullOutPath = "GameData";
        fullOutPath = fullOutPath / outputPath;
        fullOutPath = fullOutPath.parent_path();
        std::filesystem::create_directories(fullOutPath);

        PHYSFS_File* outFile = PHYSFS_openWrite(outputPath.c_str());
        PHYSFS_writeBytes(outFile, outData, outputSize);
        PHYSFS_close(outFile);

        crn_free_block(outData);

        delete inTexData.data;
        compileOp->progress = 1.0f;
        compileOp->complete = true;
    }
}
