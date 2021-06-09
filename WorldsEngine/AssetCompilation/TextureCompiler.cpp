#include "TextureCompiler.hpp"
#include "AssetCompilerUtil.hpp"
#include "../Libs/crnlib/crn_texture_conversion.h"
#include "Core/Log.hpp"
using namespace crnlib;

namespace worlds {
    AssetID TextureCompiler::compile(AssetID src) {
        std::string outputPath = getOutputPath(AssetDB::idToPath(src));
        std::string exeRelOutPath = "GameData/" + outputPath;

        texture_conversion::convert_params params;
        params.m_dst_file_type = texture_file_types::cFormatCRN;
        params.m_texture_type = texture_type::cTextureTypeRegularMap;
        params.m_dst_filename = exeRelOutPath.c_str();
        params.m_dst_format = pixel_format::PIXEL_FMT_DXT1;

        texture_conversion::convert_stats stats;
        if (!texture_conversion::process(params, stats))
            logErr("Failed to compile texture");

        return AssetDB::pathToId(outputPath);
    }

    const char* TextureCompiler::getSourceExtension() {
        return ".wtexj";
    }

    const char* TextureCompiler::getCompiledExtension() {
        return ".wtex";
    }
}
