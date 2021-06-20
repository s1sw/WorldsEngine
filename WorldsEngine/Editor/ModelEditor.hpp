#pragma once
#include "AssetEditors.hpp"

namespace worlds {
    class ModelEditor : public IAssetEditor {
    public:
        void importAsset(std::string filePath, std::string newAssetPath) override;
        void create(std::string path) override;
        void open(AssetID id) override;
        void drawEditor() override;
        void save() override;
        const char* getHandledExtension() override;
    private:
        AssetID srcModel = INVALID_ASSET;
        AssetID editingID = INVALID_ASSET;
    };
}
