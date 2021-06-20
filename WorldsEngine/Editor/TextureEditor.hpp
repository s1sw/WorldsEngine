#pragma once
#include "AssetEditors.hpp"

namespace worlds {
    class TextureEditor : public IAssetEditor {
    public:
        void importAsset(std::string filePath, std::string newAssetPath) override;
        void create(std::string path) override;
        void open(AssetID id) override;
        void drawEditor() override;
        void save() override;
        const char* getHandledExtension() override;
    private:
        enum class TextureType {
            Regular,
            NormalMap
        };
        TextureType strToTexType(std::string_view texType);

        TextureType texType = TextureType::Regular;
        bool isSrgb = true;
        int qualityLevel = 127;
        AssetID srcTexture = INVALID_ASSET;
        AssetID editingID = INVALID_ASSET;
    };
}
