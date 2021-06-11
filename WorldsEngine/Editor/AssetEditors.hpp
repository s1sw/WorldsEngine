#pragma once
#include "../Core/AssetDB.hpp"

namespace worlds {
    class IAssetEditor {
    public:
        IAssetEditor();
        virtual void create(std::string path) = 0;
        virtual void open(AssetID id) = 0;
        virtual void drawEditor() = 0;
        virtual void save() = 0;
        virtual const char* getHandledExtension() = 0;
        virtual ~IAssetEditor() {}
    };

    class AssetEditors {
    public:
        static void initialise();
        static void registerAssetEditor(IAssetEditor* editor);
        static IAssetEditor* getEditorFor(AssetID asset);
        static IAssetEditor* getEditorFor(std::string_view extension);
    private:
        struct StaticLink {
            IAssetEditor* editor;
            StaticLink* next;
        };

        static StaticLink* staticLink;
    };
}
