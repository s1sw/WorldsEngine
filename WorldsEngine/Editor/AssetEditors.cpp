#include "AssetEditors.hpp"
#include "Core/Log.hpp"
#include "robin_hood.h"
#include "../Util/Fnv.hpp"
#include <slib/List.hpp>
#include "TextureEditor.hpp"

namespace worlds {
    namespace asset_editors {
        worlds::TextureEditor te;
    }
}

namespace worlds {
    AssetEditors::StaticLink* AssetEditors::staticLink;

    slib::List<IAssetEditor*> assetEditors;
    robin_hood::unordered_flat_map<uint32_t, IAssetEditor*> assetEditorMap;

    IAssetEditor::IAssetEditor() {
        AssetEditors::registerAssetEditor(this);
    }

    void AssetEditors::initialise() {
        StaticLink* current = staticLink;
        while (current) {
            assetEditors.add(current->editor);
            uint32_t extensionHash = FnvHash(current->editor->getHandledExtension());
            assetEditorMap.insert({
                extensionHash,
                current->editor
            });

            StaticLink* tmp = current;
            current = current->next;
            delete tmp;
        }
    }

    void AssetEditors::registerAssetEditor(IAssetEditor* editor) {
        StaticLink* sl = new StaticLink {
            .editor = editor,
            .next = staticLink
        };
        staticLink = sl;
    }

    IAssetEditor* AssetEditors::getEditorFor(AssetID asset) {
        return getEditorFor(AssetDB::getAssetExtension(asset));
    }

    IAssetEditor* AssetEditors::getEditorFor(std::string_view extension) {
        uint32_t hash = FnvHash(extension);
        auto it = assetEditorMap.find(hash);

        if (it == assetEditorMap.end()) {
            return nullptr;
        }

        return it->second;
    }
}
