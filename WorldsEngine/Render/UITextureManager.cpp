#include <Core/Fatal.hpp>
#include <R2/BindlessTextureManager.hpp>
#include <R2/VKCore.hpp>
#include <Render/Loaders/TextureLoader.hpp>
#include <Render/RenderInternal.hpp>

namespace worlds
{
    VKUITextureManager::VKUITextureManager(R2::VK::Core *core, R2::BindlessTextureManager *textureManager)
        : core(core), textureManager(textureManager)
    {
        //
    }

    VKUITextureManager::~VKUITextureManager()
    {
    }

    ImTextureID VKUITextureManager::loadOrGet(AssetID id)
    {
        if (textureIds.contains(id))
        {
            return (ImTextureID)textureIds.at(id).bindlessId;
        }

        return (ImTextureID)load(id);
    }

    void VKUITextureManager::unload(AssetID id)
    {
        UITexInfo info = textureIds[id];
        delete info.tex;
        textureIds.erase(id);
    }

    uint32_t VKUITextureManager::load(AssetID id)
    {
        TextureData td = loadTexData(id);

        if (td.data == nullptr)
            fatalErr("Failed to load UI texture");

        R2::VK::TextureCreateInfo tci = R2::VK::TextureCreateInfo::Texture2D(td.format, td.width, td.height);
        R2::VK::Texture *t = core->CreateTexture(tci);

        uint32_t handle = textureManager->AllocateTextureHandle(t);

        core->QueueTextureUpload(t, td.data, td.totalDataSize);
        free(td.data);

        textureIds.insert({id, UITexInfo{t, handle}});

        return handle;
    }
}