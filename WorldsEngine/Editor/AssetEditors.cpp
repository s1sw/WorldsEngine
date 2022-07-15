#include "AssetEditors.hpp"
#include "../Util/Fnv.hpp"
#include "Core/IGameEventHandler.hpp"
#include "Core/Log.hpp"
#include "MaterialEditor.hpp"
#include "ModelEditor.hpp"
#include "TextureEditor.hpp"
#include "robin_hood.h"
#include <slib/List.hpp>

namespace worlds
{
    namespace asset_editors
    {
        TextureEditorMeta te;
        ModelEditorMeta me;
        MaterialEditorMeta mate;
    }
}

namespace worlds
{
    AssetEditors::StaticLink *AssetEditors::staticLink;

    slib::List<IAssetEditorMeta *> assetEditors;
    robin_hood::unordered_flat_map<uint32_t, IAssetEditorMeta *> assetEditorMap;

    IAssetEditorMeta::IAssetEditorMeta()
    {
        AssetEditors::registerAssetEditor(this);
    }

    void IAssetEditorMeta::setInterfaces(EngineInterfaces interfaces)
    {
        this->interfaces = interfaces;
    }

    void AssetEditors::initialise(EngineInterfaces interfaces)
    {
        StaticLink *current = staticLink;
        while (current)
        {
            assetEditors.add(current->editor);
            uint32_t extensionHash = FnvHash(current->editor->getHandledExtension());
            assetEditorMap.insert({extensionHash, current->editor});

            current->editor->setInterfaces(interfaces);

            StaticLink *tmp = current;
            current = current->next;
            delete tmp;
        }
    }

    void AssetEditors::registerAssetEditor(IAssetEditorMeta *editor)
    {
        StaticLink *sl = new StaticLink{.editor = editor, .next = staticLink};
        staticLink = sl;
    }

    IAssetEditorMeta *AssetEditors::getEditorFor(AssetID asset)
    {
        return getEditorFor(AssetDB::getAssetExtension(asset));
    }

    IAssetEditorMeta *AssetEditors::getEditorFor(std::string_view extension)
    {
        uint32_t hash = FnvHash(extension);
        auto it = assetEditorMap.find(hash);

        if (it == assetEditorMap.end())
        {
            return nullptr;
        }

        return it->second;
    }
}
