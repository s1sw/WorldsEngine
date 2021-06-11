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
        std::string exeRelOutPath = "GameData/" + outputPath;
        logMsg("Compiled %s to %s", AssetDB::idToPath(src).c_str(), exeRelOutPath.c_str());

        mipmapped_texture* inputTexture = new mipmapped_texture;

        TextureData inTexData = loadTexData(AssetDB::pathToId(j["srcPath"].get<std::string>()));

        inputTexture->init(inTexData.width, inTexData.height, 1, 1,
            pixel_format::PIXEL_FMT_A8R8G8B8,
            inputPath.c_str(), cDefaultOrientationFlags
        );

        char* c = new char[inTexData.width * inTexData.height * 4];

        // ARGB to RGB
        for (uint32_t pixelIdx = 0; pixelIdx < inTexData.totalDataSize / 4; pixelIdx++) {
            uint32_t offset = pixelIdx * 4;
            c[offset + 0] = inTexData.data[offset + 3];
            c[offset + 1] = inTexData.data[offset + 0];
            c[offset + 2] = inTexData.data[offset + 1];
            c[offset + 3] = inTexData.data[offset + 2];
        }

        image_u8* img = new image_u8((color_quad_u8*)c, inTexData.width, inTexData.height);
        inputTexture->assign(img, pixel_format::PIXEL_FMT_A8R8G8B8);

        texture_conversion::convert_params params;
        params.m_pInput_texture = inputTexture;
        params.m_dst_file_type = texture_file_types::cFormatCRN;
        params.m_texture_type = getTextureType(j.value("type", "regular"));
        params.m_dst_filename = exeRelOutPath.c_str();
        params.m_dst_format = pixel_format::PIXEL_FMT_A8R8G8B8;

        texture_conversion::convert_stats stats;
        if (!texture_conversion::process(params, stats))
            logErr("Failed to compile texture");

        delete[] c;
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
