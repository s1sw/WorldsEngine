#pragma once
#include "AssetCompilers.hpp"
#include <nlohmann/json_fwd.hpp>

namespace worlds
{
    struct CrunchTextureSettings
    {
        bool isNormalMap;
        int qualityLevel;
        bool isSrgb;
        AssetID sourceTexture;
    };

    struct RGBATextureSettings
    {
        bool isSrgb;
        AssetID sourceTexture;
    };

    struct PBRTextureSettings
    {
        AssetID roughnessSource;
        AssetID metallicSource;
        AssetID occlusionSource;
        AssetID normalMap;
        float normalRoughnessMipStrength;
        float defaultRoughness;
        float defaultMetallic;
        int qualityLevel;
    };

    enum class TextureAssetType
    {
        Crunch,
        RGBA,
        PBR
    };

    struct TextureAssetSettings
    {
        TextureAssetType type;
        union {
            CrunchTextureSettings crunch;
            RGBATextureSettings rgba;
            PBRTextureSettings pbr;
        };

        void initialiseForType(TextureAssetType t);
        static TextureAssetSettings fromJson(nlohmann::json& j);
        void toJson(nlohmann::json& j);
    };

    class TextureCompiler : public IAssetCompiler
    {
      public:
        TextureCompiler();
        AssetCompileOperation* compile(std::string_view projectRoot, AssetID src) override;
        void getFileDependencies(AssetID src, std::vector<std::string>& out) override;
        const char* getSourceExtension() override;
        const char* getCompiledExtension() override;

      private:
        struct TexCompileThreadInfo;
        void compileCrunch(TexCompileThreadInfo*);
        void compileRGBA(TexCompileThreadInfo*);
        void compilePBR(TexCompileThreadInfo*);
        void writeCrunchedWtex(TexCompileThreadInfo* tcti, bool isSrgb, int width, int height, int nMips, void* data,
                               size_t dataSize);
        void compileInternal(TexCompileThreadInfo*);
    };
}
