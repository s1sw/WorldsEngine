#include <Core/Fatal.hpp>
#include <R2/BindlessTextureManager.hpp>
#include <R2/VKCore.hpp>
#include <Render/Loaders/TextureLoader.hpp>
#include <Render/RenderInternal.hpp>

namespace worlds
{
    VKUITextureManager::VKUITextureManager(VKTextureManager* texMan)
        : texMan(texMan)
    {
    }

    ImTextureID VKUITextureManager::loadOrGet(AssetID id)
    {
        return (ImTextureID)texMan->loadOrGet(id);
    }

    void VKUITextureManager::unload(AssetID id)
    {
        texMan->unload(id);
    }

    VKTextureManager::VKTextureManager(R2::VK::Core* core, R2::BindlessTextureManager* textureManager)
        : core(core), textureManager(textureManager)
    {
    }

    VKTextureManager::~VKTextureManager()
    {
        for (auto& pair : textureIds)
        {
            delete pair.second.tex;
            textureManager->FreeTextureHandle(pair.second.bindlessId);
        }
    }

    uint32_t VKTextureManager::loadOrGet(AssetID id)
    {
        if (textureIds.contains(id))
        {
            return textureIds.at(id).bindlessId;
        }

        return load(id);
    }

    void VKTextureManager::unload(AssetID id)
    {
        TexInfo info = textureIds[id];
        delete info.tex;
        textureIds.erase(id);
        textureManager->FreeTextureHandle(info.bindlessId);
    }

    uint32_t VKTextureManager::load(AssetID id)
    {
        TextureData td = loadTexData(id);

        if (td.data == nullptr)
            fatalErr("Failed to load texture");

        R2::VK::TextureCreateInfo tci = R2::VK::TextureCreateInfo::Texture2D(td.format, td.width, td.height);
        R2::VK::Texture* t = core->CreateTexture(tci);

        uint32_t handle = textureManager->AllocateTextureHandle(t);

        core->QueueTextureUpload(t, td.data, td.totalDataSize);
        free(td.data);

        textureIds.insert({id, TexInfo{t, handle}});

        return handle;
    }
}