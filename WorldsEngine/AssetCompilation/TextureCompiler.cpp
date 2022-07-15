#define CRND_HEADER_FILE_ONLY
#include "TextureCompiler.hpp"
#include "AssetCompilerUtil.hpp"
#include "RawTextureLoader.hpp"
#include <Core/Log.hpp>
#include <IO/IOUtil.hpp>
#include <Libs/crnlib/crn_texture_conversion.h>
#include <SDL_cpuinfo.h>
#include <WTex.hpp>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <thread>
using namespace crnlib;

namespace worlds
{
    TextureCompiler::TextureCompiler()
    {
        AssetCompilers::registerCompiler(this);
    }

    texture_type getTextureType(std::string typeStr)
    {
        if (typeStr == "regular")
        {
            return cTextureTypeRegularMap;
        }
        else if (typeStr == "normal")
        {
            return cTextureTypeNormalMap;
        }
        else
        {
            logErr("Invalid texture type: %s", typeStr.c_str());
            return cTextureTypeRegularMap;
        }
    }

    struct TextureCompiler::TexCompileThreadInfo
    {
        TextureAssetSettings assetSettings;
        std::string inputPath;
        std::string outputPath;
        std::string_view projectRoot;
        AssetCompileOperation *compileOp;
    };

    TextureAssetType getAssetType(std::string typeStr)
    {
        if (typeStr == "crunch")
        {
            return TextureAssetType::Crunch;
        }
        else if (typeStr == "rgba")
        {
            return TextureAssetType::RGBA;
        }
        else if (typeStr == "pbr")
        {
            return TextureAssetType::PBR;
        }
        else
        {
            logWarn("Invalid texture asset type: %s", typeStr.c_str());
            return TextureAssetType::RGBA;
        }
    }

    std::string getAssetTypeString(TextureAssetType t)
    {
        switch (t)
        {
        case TextureAssetType::Crunch:
            return "crunch";
        case TextureAssetType::RGBA:
            return "rgba";
        case TextureAssetType::PBR:
            return "pbr";
        }
    }

    void TextureAssetSettings::initialiseForType(TextureAssetType t)
    {
        type = t;
        switch (t)
        {
        case TextureAssetType::Crunch:
            crunch.sourceTexture = INVALID_ASSET;
            crunch.isNormalMap = false;
            crunch.isSrgb = true;
            crunch.qualityLevel = 127;
            break;
        case TextureAssetType::RGBA:
            rgba.sourceTexture = INVALID_ASSET;
            rgba.isSrgb = true;
            break;
        case TextureAssetType::PBR:
            pbr.roughnessSource = INVALID_ASSET;
            pbr.metallicSource = INVALID_ASSET;
            pbr.occlusionSource = INVALID_ASSET;
            pbr.normalMap = INVALID_ASSET;
            pbr.normalRoughnessMipStrength = 0.0f;
            pbr.defaultRoughness = 0.5f;
            pbr.defaultMetallic = 0.0f;
            pbr.qualityLevel = 127;
            break;
        }
    }

    void loadCrunchSettings(TextureAssetSettings &settings, nlohmann::json &j)
    {
        settings.crunch.isNormalMap = j.value("isNormalMap", false);
        settings.crunch.qualityLevel = j.value("qualityLevel", 127);
        settings.crunch.isSrgb = j.value("isSrgb", true);
        if (j.contains("sourceTexture"))
            settings.crunch.sourceTexture = AssetDB::pathToId(j["sourceTexture"].get<std::string>());
        else
            settings.crunch.sourceTexture = INVALID_ASSET;
    }

    void loadRGBASettings(TextureAssetSettings &settings, nlohmann::json &j)
    {
        settings.rgba.sourceTexture = AssetDB::pathToId(j["sourceTexture"].get<std::string>());
        settings.rgba.isSrgb = j.value("isSrgb", true);
    }

    void loadPBRSettings(TextureAssetSettings &settings, nlohmann::json &j)
    {
        settings.pbr.defaultMetallic = j.value("defaultMetallic", 0.0f);
        settings.pbr.defaultRoughness = j.value("defaultRoughness", 0.0f);

        if (j.contains("metallicSource"))
            settings.pbr.metallicSource = AssetDB::pathToId(j["metallicSource"].get<std::string>());
        else
            settings.pbr.metallicSource = INVALID_ASSET;

        if (j.contains("roughnessSource"))
            settings.pbr.roughnessSource = AssetDB::pathToId(j["roughnessSource"].get<std::string>());
        else
            settings.pbr.roughnessSource = INVALID_ASSET;

        if (j.contains("occlusionSource"))
            settings.pbr.occlusionSource = AssetDB::pathToId(j["occlusionSource"].get<std::string>());
        else
            settings.pbr.occlusionSource = INVALID_ASSET;

        if (j.contains("normalMap"))
            settings.pbr.normalMap = AssetDB::pathToId(j["normalMap"].get<std::string>());
        else
            settings.pbr.normalMap = INVALID_ASSET;

        settings.pbr.normalRoughnessMipStrength = j.value("normalRoughnessMipStrength", 0.0f);
        settings.pbr.qualityLevel = j.value("qualityLevel", 127);
    }

    TextureAssetSettings TextureAssetSettings::fromJson(nlohmann::json &j)
    {
        TextureAssetSettings s{};

        s.type = getAssetType(j.value("type", "crunch"));

        switch (s.type)
        {
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

    void saveCrunchSettings(TextureAssetSettings &settings, nlohmann::json &j)
    {
        CrunchTextureSettings &cts = settings.crunch;
        j["isNormalMap"] = cts.isNormalMap;
        j["qualityLevel"] = cts.qualityLevel;
        j["isSrgb"] = cts.isSrgb;
        j["sourceTexture"] = AssetDB::idToPath(cts.sourceTexture);
    }

    void savePBRSettings(TextureAssetSettings &settings, nlohmann::json &j)
    {
        PBRTextureSettings &pbs = settings.pbr;
        j["defaultMetallic"] = pbs.defaultMetallic;
        j["defaultRoughness"] = pbs.defaultRoughness;

        if (pbs.metallicSource != INVALID_ASSET)
        {
            j["metallicSource"] = AssetDB::idToPath(pbs.metallicSource);
        }

        if (pbs.roughnessSource != INVALID_ASSET)
        {
            j["roughnessSource"] = AssetDB::idToPath(pbs.roughnessSource);
        }

        if (pbs.occlusionSource != INVALID_ASSET)
        {
            j["occlusionSource"] = AssetDB::idToPath(pbs.occlusionSource);
        }

        j["normalRoughnessMipStrength"] = pbs.normalRoughnessMipStrength;
        j["qualityLevel"] = pbs.qualityLevel;
    }

    void TextureAssetSettings::toJson(nlohmann::json &j)
    {
        j["type"] = getAssetTypeString(type);

        switch (type)
        {
        case TextureAssetType::Crunch:
            saveCrunchSettings(*this, j);
            break;
        case TextureAssetType::RGBA:
            break;
        case TextureAssetType::PBR:
            savePBRSettings(*this, j);
            break;
        }
    }

    AssetCompileOperation *TextureCompiler::compile(std::string_view projectRoot, AssetID src)
    {
        std::string inputPath = AssetDB::idToPath(src);
        std::string outputPath = getOutputPath(inputPath);

        auto jsonContents = LoadFileToString(inputPath);
        AssetCompileOperation *compileOp = new AssetCompileOperation;

        if (jsonContents.error != IOError::None)
        {
            logErr("Error opening asset file");
            compileOp->complete = true;
            compileOp->result = CompilationResult::Error;
            return compileOp;
        }

        nlohmann::json j = nlohmann::json::parse(jsonContents.value);
        compileOp->outputId = AssetDB::pathToId(outputPath);
        TexCompileThreadInfo *tcti = new TexCompileThreadInfo;
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

    void TextureCompiler::getFileDependencies(AssetID src, std::vector<std::string> &out)
    {
        std::string inputPath = AssetDB::idToPath(src);
        auto jsonContents = LoadFileToString(inputPath);

        if (jsonContents.error != IOError::None)
        {
            logErr("Error opening asset file");
            return;
        }

        nlohmann::json j = nlohmann::json::parse(jsonContents.value);

        TextureAssetSettings tas = TextureAssetSettings::fromJson(j);

        switch (tas.type)
        {
        case TextureAssetType::Crunch:
            out.push_back(j["sourceTexture"]);
            break;
        case TextureAssetType::PBR:
            if (j.contains("metallicSource"))
                out.push_back(j["metallicSource"]);

            if (j.contains("roughnessSource"))
                out.push_back(j["roughnessSource"]);

            if (j.contains("occlusionSource"))
                out.push_back(j["occlusionSource"]);
            break;
        }
    }

    const char *TextureCompiler::getSourceExtension()
    {
        return ".wtexj";
    }

    const char *TextureCompiler::getCompiledExtension()
    {
        return ".wtex";
    }

    crn_bool progressCallback(crn_uint32 phaseIndex, crn_uint32 totalPhases, crn_uint32 subphaseIndex,
                              crn_uint32 totalSubphases, void *data)
    {
        AssetCompileOperation *compileOp = (AssetCompileOperation *)data;
        compileOp->progress = ((float)phaseIndex + ((float)subphaseIndex / totalSubphases)) / totalPhases;
        return true;
    }

    void TextureCompiler::compileCrunch(TexCompileThreadInfo *tcti)
    {
        const CrunchTextureSettings &cts = tcti->assetSettings.crunch;
        AssetID sourceId = cts.sourceTexture;
        bool isSrgb = cts.isSrgb;
        RawTextureData inTexData;

        if (!RawTextureLoader::loadRawTexture(sourceId, inTexData))
        {
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
        compParams.m_pImages[0][0] = (crn_uint32 *)inTexData.data;
        compParams.m_quality_level = cts.qualityLevel;
        compParams.m_num_helper_threads = SDL_GetCPUCount() - 1;
        compParams.m_pProgress_func = progressCallback;
        compParams.m_pProgress_func_data = tcti->compileOp;
        compParams.m_userdata0 = isSrgb;
        logMsg("perceptual: %i, format: %i", compParams.get_flag(cCRNCompFlagPerceptual), compParams.m_format);

        if (inTexData.format == RawTextureFormat::RGBA32F)
        {
            // Need to convert to an array of u8s
            uint8_t *newData = (uint8_t *)malloc(inTexData.width * inTexData.height * 4);

            for (int x = 0; x < inTexData.width; x++)
            {
                for (int y = 0; y < inTexData.height; y++)
                {
                    int pixelIndex = x + (y * inTexData.width);
                    for (int c = 0; c < 4; c++)
                    {
                        int realIndex = c + pixelIndex * 4;
                        newData[realIndex] = ((float *)inTexData.data)[realIndex] * 255.f;
                    }
                }
            }

            compParams.m_pImages[0][0] = (crn_uint32 *)newData;
        }

        crn_mipmap_params mipParams;
        mipParams.m_gamma_filtering = isSrgb;
        mipParams.m_mode = cCRNMipModeGenerateMips;

        crn_uint32 outputSize;
        float actualBitrate;
        crn_uint32 actualQualityLevel;

        void *outData = crn_compress(compParams, mipParams, outputSize, &actualQualityLevel, &actualBitrate);
        logMsg("Compressed %s with actual quality of %u and bitrate of %f", tcti->inputPath.c_str(), actualQualityLevel,
               actualBitrate);

        int nMips = std::floor(std::log2(std::max(inTexData.width, inTexData.height))) + 1;

        writeCrunchedWtex(tcti, isSrgb, inTexData.width, inTexData.height, nMips, outData, outputSize);

        crn_free_block(outData);

        free(inTexData.data);
        tcti->compileOp->progress = 1.0f;
        tcti->compileOp->complete = true;
        tcti->compileOp->result = CompilationResult::Success;

        if (inTexData.format == RawTextureFormat::RGBA32F)
        {
            // Free the previously allocated temp array
            free((void *)compParams.m_pImages[0][0]);
        }
    }

    void TextureCompiler::compilePBR(TexCompileThreadInfo *tcti)
    {
        assert(tcti->assetSettings.type == TextureAssetType::PBR);
        const PBRTextureSettings &pts = tcti->assetSettings.pbr;

        RawTextureData roughnessData;
        RawTextureData metallicData;
        RawTextureData occlusionData;
        bool useRoughness = false;
        bool useMetallic = false;
        bool useOcclusion = false;

        if (pts.roughnessSource != INVALID_ASSET)
        {
            if (RawTextureLoader::loadRawTexture(pts.roughnessSource, roughnessData))
            {
                useRoughness = true;
            }
            else
            {
                logErr("Failed to load roughness layer");
            }
        }

        if (pts.metallicSource != INVALID_ASSET)
        {
            if (RawTextureLoader::loadRawTexture(pts.metallicSource, metallicData))
            {
                useMetallic = true;
            }
            else
            {
                logErr("Failed to load metallic layer");
            }
        }

        if (pts.occlusionSource != INVALID_ASSET)
        {
            if (RawTextureLoader::loadRawTexture(pts.occlusionSource, occlusionData))
            {
                useOcclusion = true;
            }
            else
            {
                logErr("Failed to load occlusion layer");
            }
        }

        int outWidth = roughnessData.width;
        int outHeight = roughnessData.height;

        if (useMetallic)
        {
            if (metallicData.width != outWidth || metallicData.height != outHeight)
                logErr("Metallic texture size doesn't match!");
        }

        uint8_t *layeredData = new uint8_t[outWidth * outHeight * 4];

        for (int x = 0; x < outWidth; x++)
        {
            for (int y = 0; y < outHeight; y++)
            {
                int baseIdx = 4 * (x + y * outWidth);

                // Red channel - Metallic
                if (useMetallic)
                    layeredData[baseIdx + 0] = static_cast<uint8_t *>(metallicData.data)[baseIdx + 0];
                else
                    layeredData[baseIdx + 0] = 255 * pts.defaultMetallic;

                // Green channel - Roughness
                if (useRoughness)
                    layeredData[baseIdx + 1] = static_cast<uint8_t *>(roughnessData.data)[baseIdx + 1];
                else
                    layeredData[baseIdx + 1] = 255 * pts.defaultRoughness;

                // Red channel - Occlusion
                if (useOcclusion)
                    layeredData[baseIdx + 2] = static_cast<uint8_t *>(occlusionData.data)[baseIdx + 2];
                else
                    layeredData[baseIdx + 2] = 255;

                // Alpha channel is currently unused, set it to 255
                layeredData[baseIdx + 3] = 255;
            }
        }

        crn_comp_params compParams;
        compParams.m_width = outWidth;
        compParams.m_height = outHeight;
        compParams.set_flag(cCRNCompFlagPerceptual, false);
        compParams.m_file_type = cCRNFileTypeCRN;
        compParams.m_format = cCRNFmtDXT5;
        compParams.m_pImages[0][0] = (crn_uint32 *)layeredData;
        compParams.m_quality_level = pts.qualityLevel;
        compParams.m_num_helper_threads = SDL_GetCPUCount() - 1;
        compParams.m_pProgress_func = progressCallback;
        compParams.m_pProgress_func_data = tcti->compileOp;
        compParams.m_userdata0 = false;
        logMsg("perceptual: %i, format: %i", compParams.get_flag(cCRNCompFlagPerceptual), compParams.m_format);

        crn_mipmap_params mipParams;
        mipParams.m_gamma_filtering = false;
        mipParams.m_mode = cCRNMipModeGenerateMips;

        crn_uint32 outputSize;
        float actualBitrate;
        crn_uint32 actualQualityLevel;

        void *outData = crn_compress(compParams, mipParams, outputSize, &actualQualityLevel, &actualBitrate);
        logMsg("Compressed %s with actual quality of %u and bitrate of %f", tcti->inputPath.c_str(), actualQualityLevel,
               actualBitrate);

        int nMips = std::floor(std::log2(std::max(outWidth, outHeight))) + 1;

        writeCrunchedWtex(tcti, false, outWidth, outHeight, nMips, outData, outputSize);

        crn_free_block(outData);

        delete[] layeredData;

        tcti->compileOp->progress = 1.0f;
        tcti->compileOp->complete = true;
        tcti->compileOp->result = CompilationResult::Success;
    }

    void TextureCompiler::writeCrunchedWtex(TexCompileThreadInfo *tcti, bool isSrgb, int width, int height, int nMips,
                                            void *data, size_t dataSize)
    {
        wtex::Header header{};
        header.containedFormat = wtex::ContainedFormat::Crunch;
        header.dataOffset = sizeof(header);
        header.dataSize = dataSize;
        header.width = width;
        header.height = height;
        header.numMipLevels = nMips;
        header.isSrgb = isSrgb;

        std::filesystem::path fullPath = tcti->projectRoot;
        fullPath /= tcti->outputPath;
        fullPath = fullPath.parent_path();
        fullPath = fullPath.lexically_normal();

        std::filesystem::create_directories(fullPath);

        PHYSFS_File *outFile = PHYSFS_openWrite(tcti->outputPath.c_str());
        PHYSFS_writeBytes(outFile, &header, sizeof(header));
        PHYSFS_writeBytes(outFile, data, dataSize);
        PHYSFS_close(outFile);
    }

    void TextureCompiler::compileInternal(TexCompileThreadInfo *tcti)
    {
        switch (tcti->assetSettings.type)
        {
        case TextureAssetType::Crunch:
            compileCrunch(tcti);
            break;
        case TextureAssetType::RGBA:
            logErr("Currently unsupported :(");
            tcti->compileOp->complete = true;
            tcti->compileOp->result = CompilationResult::Error;
            break;
        case TextureAssetType::PBR:
            compilePBR(tcti);
            break;
        }
    }
}
