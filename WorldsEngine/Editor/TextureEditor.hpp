#pragma once
#include "AssetEditors.hpp"
#include "Core/IGameEventHandler.hpp"
#include <AssetCompilation/TextureCompiler.hpp>

namespace worlds {
    class TextureEditor : public IAssetEditor {
    public:
        TextureEditor(AssetID id);
        void draw() override;
        void save() override;
        ~TextureEditor();
    private:
        TextureAssetSettings currentAssetSettings;
        AssetID editingID = INVALID_ASSET;
    };

    class TextureEditorMeta : public IAssetEditorMeta {
    public:
        void importAsset(std::string filePath, std::string newAssetPath) override;
        void create(std::string path) override;
        IAssetEditor* createEditorFor(AssetID id) override;
        const char* getHandledExtension() override;
    private:
    };
}
