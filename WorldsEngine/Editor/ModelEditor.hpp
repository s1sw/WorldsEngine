#pragma once
#include "AssetEditors.hpp"

namespace worlds
{
    class ModelEditor : public IAssetEditor
    {
      public:
        ModelEditor(AssetID id);
        void draw() override;
        void save() override;
        bool hasUnsavedChanges() override;

      private:
        AssetID editingID;
        AssetID srcModel = INVALID_ASSET;
        bool preTransformVerts = false;
        bool removeRedundantMaterials = true;
        float uniformScale = 1.0f;
        bool unsavedChanges = false;
    };

    class ModelEditorMeta : public IAssetEditorMeta
    {
      public:
        void importAsset(std::string filePath, std::string newAssetPath) override;
        void create(std::string path) override;
        IAssetEditor *createEditorFor(AssetID id) override;
        const char *getHandledExtension() override;

      private:
    };
}
