#pragma once
#include "AssetEditors.hpp"
#include <AssetCompilation/TextureCompiler.hpp>

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
        TextureAssetSettings currentAssetSettings;
        AssetID editingID = INVALID_ASSET;
    };
}
