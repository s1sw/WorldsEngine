#define CRND_HEADER_FILE_ONLY
#include "TextureCompiler.hpp"
#include "AssetCompilerUtil.hpp"
#include <Libs/crnlib/crn_texture_conversion.h>
#include <Core/Log.hpp>
#include <Render/Loaders/TextureLoader.hpp>
#include <nlohmann/json.hpp>
#include <IO/IOUtil.hpp>
#include <filesystem>
#include <SDL_cpuinfo.h>
#include <thread>
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

    struct TextureCompiler::TexCompileThreadInfo {
        nlohmann::json j;
        std::string inputPath;
        std::string outputPath;
        std::string_view projectRoot;
        AssetCompileOperation* compileOp;
    };

    AssetCompileOperation* TextureCompiler::compile(std::string_view projectRoot, AssetID src) {
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
        tcti->projectRoot = projectRoot;

        std::thread([tcti, this]() {
            compileInternal(tcti);
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
        compileOp->progress = ((float)phaseIndex + ((float)subphaseIndex / totalSubphases)) / totalPhases;
        return true;
    }
    

    void TextureCompiler::compileInternal(TexCompileThreadInfo* tcti) {
        auto& j = tcti->j;
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
        compParams.m_quality_level = j.value("qualityLevel", 127);
        compParams.m_num_helper_threads = SDL_GetCPUCount() - 1;
        compParams.m_pProgress_func = progressCallback;
        compParams.m_pProgress_func_data = tcti->compileOp;
        compParams.m_userdata0 = isSrgb;
        logMsg("perceptual: %i, format: %i", compParams.get_flag(cCRNCompFlagPerceptual), compParams.m_format);

        crn_mipmap_params mipParams;
        mipParams.m_gamma_filtering = true;
        mipParams.m_mode = cCRNMipModeGenerateMips;

        crn_uint32 outputSize;
        float actualBitrate;
        crn_uint32 actualQualityLevel;

        void* outData = crn_compress(compParams, mipParams, outputSize, &actualQualityLevel, &actualBitrate);
        logMsg("Compressed %s with actual quality of %u and bitrate of %f", tcti->inputPath.c_str(), actualQualityLevel, actualBitrate);

        std::filesystem::path fullPath = tcti->projectRoot;
        fullPath /= tcti->outputPath;
        fullPath = fullPath.parent_path();
        fullPath = fullPath.lexically_normal();

        std::filesystem::create_directories(fullPath);

        // TODO: Assumes that we are writing to the GameData directory! BADDD!!!!
        PHYSFS_File* outFile = PHYSFS_openWrite(tcti->outputPath.c_str());
        PHYSFS_writeBytes(outFile, outData, outputSize);
        PHYSFS_close(outFile);

        crn_free_block(outData);

        delete inTexData.data;
        tcti->compileOp->progress = 1.0f;
        tcti->compileOp->complete = true;
    }
}
