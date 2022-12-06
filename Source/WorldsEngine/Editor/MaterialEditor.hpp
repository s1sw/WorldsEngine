#pragma once
#include "AssetEditors.hpp"
#include "Core/AssetDB.hpp"
#include "Render/Camera.hpp"
#include "Render/Render.hpp"
#include <Core/IGameEventHandler.hpp>
#include <entt/entity/registry.hpp>

namespace worlds
{
    struct EditableMaterial
    {
        AssetID albedo = ~0u;
        AssetID normalMap = ~0u;
        AssetID metalMap = ~0u;
        AssetID roughMap = ~0u;
        AssetID heightMap = ~0u;
        AssetID pbrMap = ~0u;
        glm::vec3 albedoColor = glm::vec3{1.0f, 1.0f, 1.0f};
        glm::vec3 emissiveColor = glm::vec3{0.0f, 0.0f, 0.0f};
        float metallic = 0.0f;
        float roughness = 0.75f;
        float alphaCutoff = 0.0f;
        float heightmapScale = 0.0f;
        bool cullOff = false;
        bool wireframe = false;
        bool usePBRMap = false;
        bool useAlphaTest = false;
    };

    class MaterialEditor : public IAssetEditor
    {
      public:
        MaterialEditor(AssetID id, EngineInterfaces interfaces);
        void draw() override;
        void save() override;
        bool hasUnsavedChanges() override;
        ~MaterialEditor();

      private:
        AssetID editingID = INVALID_ASSET;
        EngineInterfaces interfaces;
        RTTPass* rttPass;
        Camera previewCam;
        entt::entity previewEntity;
        entt::registry previewRegistry;
        EditableMaterial mat;
        bool dragging = false;
        float lx;
        float ly;
        float dist;
        bool unsavedChanges = false;
    };

    class MaterialEditorMeta : public IAssetEditorMeta
    {
      public:
        void importAsset(std::string filePath, std::string newAssetPath) override;
        void create(std::string path) override;
        IAssetEditor* createEditorFor(AssetID id) override;
        const char* getHandledExtension() override;
    };
}
