#pragma once
#include <Core/AssetDB.hpp>
#include <Core/IGameEventHandler.hpp>

namespace worlds
{
    class IAssetEditor
    {
      public:
        virtual void draw() = 0;
        virtual void save() = 0;
        virtual bool hasUnsavedChanges() = 0;
        virtual ~IAssetEditor()
        {
        }
    };

    class IAssetEditorMeta
    {
      public:
        IAssetEditorMeta();
        void setInterfaces(EngineInterfaces interfaces);
        virtual void importAsset(std::string filePath, std::string newAssetPath) = 0;
        virtual void create(std::string path) = 0;
        virtual IAssetEditor* createEditorFor(AssetID id) = 0;
        virtual const char* getHandledExtension() = 0;
        virtual ~IAssetEditorMeta()
        {
        }

      protected:
        EngineInterfaces interfaces;
    };

    class AssetEditors
    {
      public:
        static void initialise(EngineInterfaces interfaces);
        static void registerAssetEditor(IAssetEditorMeta* editor);
        static IAssetEditorMeta* getEditorFor(AssetID asset);
        static IAssetEditorMeta* getEditorFor(std::string_view extension);

      private:
        struct StaticLink
        {
            IAssetEditorMeta* editor;
            StaticLink* next;
        };

        static StaticLink* staticLink;
    };
}
