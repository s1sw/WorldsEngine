#define CRND_HEADER_FILE_ONLY
#include "TextureCompiler.hpp"
#include "AssetCompilerUtil.hpp"
#include <Libs/crnlib/crn_texture_conversion.h>
#include <Core/Log.hpp>
#include <nlohmann/json.hpp>
#include <IO/IOUtil.hpp>
#include <filesystem>
#include <SDL_cpuinfo.h>
#include <thread>
#include "RawTextureLoader.hpp"
#include <WTex.hpp>
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
        TextureAssetSettings assetSettings;
        std::string inputPath;
        std::string outputPath;
        std::string_view projectRoot;
        AssetCompileOperation* compileOp;
    };

    TextureAssetType getAssetType(std::string typeStr) {
        if (typeStr == "crunch") {
            return TextureAssetType::Crunch;
        } else if (typeStr == "rgba") {
            return TextureAssetType::RGBA;
        } else if (typeStr == "pbr") {
            return TextureAssetType::PBR;
        } else {
            logWarn("Invalid texture asset type: %s", typeStr.c_str());
            return TextureAssetType::RGBA;
        }
    }

    void loadCrunchSettings(TextureAssetSettings& settings, nlohmann::json& j) {
        settings.crunch.isNormalMap = j.value("isNormalMap", true);
        settings.crunch.qualityLevel = j.value("qualityLevel", 127);
        settings.crunch.isSrgb = j.value("isSrgb", true);
        settings.crunch.sourceTexture = AssetDB::pathToId(j["sourceTexture"].get<std::string>());
    }

    void loadRGBASettings(TextureAssetSettings& settings, nlohmann::json& j) {
        settings.rgba.sourceTexture = AssetDB::pathToId(j["sourceTexture"].get<std::string>());
        settings.rgba.isSrgb = j.value("isSrgb", true);
    }

    void loadPBRSettings(TextureAssetSettings& settings, nlohmann::json& j) {
        settings.pbr.defaultMetallic = j.value("defaultMetallic", 0.0f);
        settings.pbr.defaultRoughness = j.value("defaultRoughness", 0.0f);
        settings.pbr.defaultOcclusion = j.value("defaultOcclusion", 0.0f);
        settings.pbr.metallicSource = AssetDB::pathToId(j["metallicSource"].get<std::string>());
        settings.pbr.roughnessSource = AssetDB::pathToId(j["roughnessSource"].get<std::string>());
        settings.pbr.occlusionSource = AssetDB::pathToId(j["occlusionSource"].get<std::string>());
        settings.pbr.normalMap = AssetDB::pathToId(j["normalMap"].get<std::string>());
        settings.pbr.normalRoughnessMipStrength = j.value("normalRoughnessMipStrength", 0.0f);
    }

    TextureAssetSettings TextureAssetSettings::fromJson(nlohmann::json& j) {
        TextureAssetSettings s{};

        s.type = getAssetType(j["type"]);

        switch (s.type) {
        case TextureAssetType::Crunch:
            loadCrunchSettings(s, j);
            break;
        case TextureAssetType::RGBA:
            loadRGBASettings(s, j);
            break;
        case TextureAssetType::PBR:
            loadPBRSettings(s, j);
            break;
        }

        return s;
    }

    AssetCompileOperation* TextureCompiler::compile(std::string_view projectRoot, AssetID src) {
        std::string inputPath = AssetDB::idToPath(src);
        std::string outputPath = getOutputPath(inputPath);

        auto jsonContents = LoadFileToString(inputPath);
        AssetCompileOperation* compileOp = new AssetCompileOperation;

        if (jsonContents.error != IOError::None) {
            logErr("Error opening asset file");
            compileOp->complete = true;
            compileOp->result = CompilationResult::Error;
            return compileOp;
        }

        nlohmann::json j = nlohmann::json::parse(jsonContents.value);
        compileOp->outputId = AssetDB::pathToId(outputPath);
        TexCompileThreadInfo* tcti = new TexCompileThreadInfo;
        tcti->assetSettings = TextureAssetSettings::fromJson(j);
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

    void TextureCompiler::getFileDependencies(AssetID src, std::vector<std::string>& out) {
        std::string inputPath = AssetDB::idToPath(src);
        auto jsonContents = LoadFileToString(inputPath);

        if (jsonContents.error != IOError::None) {
            logErr("Error opening asset file");
            return;
        }

        nlohmann::json j = nlohmann::json::parse(jsonContents.value);
        out.push_back(j["srcPath"]);
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

    void TextureCompiler::compileCrunch(TexCompileThreadInfo* tcti) {
        const CrunchTextureSettings& cts = tcti->assetSettings.crunch;
        AssetID sourceId = cts.sourceTexture; 
        bool isSrgb = cts.isSrgb;
        RawTextureData inTexData;

        if (!RawTextureLoader::loadRawTexture(sourceId, inTexData)) {
            logErr("Failed to compile %s", AssetDB::idToPath(tcti->compileOp->outputId));
            return;
        }
        logMsg("Texture is %ix%i", inTexData.width, inTexData.height);

        crn_comp_params compParams;
        compParams.m_width = inTexData.width;
        compParams.m_height = inTexData.height;
        compParams.set_flag(cCRNCompFlagPerceptual, isSrgb);
        compParams.m_file_type = cCRNFileTypeCRN;
        compParams.m_format = cts.isNormalMap ? cCRNFmtDXN_XY : cCRNFmtDXT5;
        compParams.m_pImages[0][0] = (crn_uint32*)inTexData.data;
        compParams.m_quality_level = cts.qualityLevel;
        compParams.m_num_helper_threads = SDL_GetCPUCount() - 1;
        compParams.m_pProgress_func = progressCallback;
        compParams.m_pProgress_func_data = tcti->compileOp;
        compParams.m_userdata0 = isSrgb;
        logMsg("perceptual: %i, format: %i", compParams.get_flag(cCRNCompFlagPerceptual), compParams.m_format);

        if (inTexData.format == RawTextureFormat::RGBA32F) {
            // Need to convert to an array of u8s
            uint8_t* newData = (uint8_t*)malloc(inTexData.width * inTexData.height * 4);

            for (int x = 0; x < inTexData.width; x++) {
                for (int y = 0; y < inTexData.height; y++) {
                    int pixelIndex = x + (y * inTexData.width);
                    for (int c = 0; c < 4; c++) {
                        int realIndex = c + pixelIndex * 4;
                        newData[realIndex] = ((float*)inTexData.data)[realIndex] * 255.f;
                    }
                }
            }

            compParams.m_pImages[0][0] = (crn_uint32*)newData;
        }

        crn_mipmap_params mipParams;
        mipParams.m_gamma_filtering = isSrgb;
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

        free(inTexData.data);
        tcti->compileOp->progress = 1.0f;
        tcti->compileOp->complete = true;
        tcti->compileOp->result = CompilationResult::Success;

        if (inTexData.format == RawTextureFormat::RGBA32F) {
            // Free the previously allocated temp array
            free((void*)compParams.m_pImages[0][0]);
        }
    }

    void TextureCompiler::compileInternal(TexCompileThreadInfo* tcti) {
        switch (tcti->assetSettings.type) {
        case TextureAssetType::Crunch:
            compileCrunch(tcti);
            break;
        case TextureAssetType::RGBA:
            break;
        case TextureAssetType::PBR:
            break;
        }
    }
}
