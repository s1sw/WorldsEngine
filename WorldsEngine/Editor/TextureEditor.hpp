#pragma once
#include "AssetEditors.hpp"
#include <nlohmann/json_fwd.hpp>

namespace worlds {
    class TextureEditor : public IAssetEditor {
    public:
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
        AssetID srcTexture = INVALID_ASSET;
        AssetID editingID = INVALID_ASSET;
    };
}
