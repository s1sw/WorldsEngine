#pragma once
#include "AssetEditors.hpp"
#include "Render/Camera.hpp"
#include "Render/Render.hpp"
#include <Core/IGameEventHandler.hpp>

namespace worlds {
    class MaterialEditor : public IAssetEditor {
    public:
        void setInterfaces(EngineInterfaces interfaces) override;
        void importAsset(std::string filePath, std::string newAssetPath) override;
        void create(std::string path) override;
        void open(AssetID id) override;
        void drawEditor() override;
        void save() override;
        const char* getHandledExtension() override;
    private:
        AssetID editingID = INVALID_ASSET;
        EngineInterfaces interfaces;
        RTTPass* rttPass;
        Camera previewCam;
        entt::entity previewEntity;
    };
}
