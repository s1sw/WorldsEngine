#include <Core/Fatal.hpp>
#include <Core/TaskScheduler.hpp>
#include <R2/BindlessTextureManager.hpp>
#include <R2/VKCore.hpp>
#include <Render/Loaders/TextureLoader.hpp>
#include <Render/RenderInternal.hpp>
#include <ImGui/imgui.h>

namespace worlds
{
    VKUITextureManager::VKUITextureManager(VKTextureManager* texMan)
        : texMan(texMan)
    {
    }

    ImTextureID VKUITextureManager::loadOrGet(AssetID id)
    {
        return (ImTextureID)(uint64_t)texMan->loadAndGet(id);
    }

    void VKUITextureManager::unload(AssetID id)
    {
        texMan->unload(id);
    }

    VKTextureManager::VKTextureManager(R2::VK::Core* core, R2::BindlessTextureManager* textureManager)
        : core(core), textureManager(textureManager)
    {
        missingTextureID = loadAndGet(AssetDB::pathToId("Textures/missing.wtex"));
        missingTexture = textureManager->GetTextureAt(missingTextureID);
    }

    VKTextureManager::~VKTextureManager()
    {
        for (auto& pair : textureIds)
        {
            delete pair.second.tex;
            textureManager->FreeTextureHandle(pair.second.bindlessId);
        }
    }

    uint32_t VKTextureManager::loadAndGet(AssetID id)
    {
        std::unique_lock lock{idMutex};
        if (textureIds.contains(id))
        {
            textureIds.at(id).refCount++;
            return textureIds.at(id).bindlessId;
        }

        // Allocate a handle while we still have the lock but without actual
        // texture data. This stops other threads from trying to load the same
        // texture
        uint32_t handle = textureManager->AllocateTextureHandle(nullptr);
        textureIds.insert({id, TexInfo{nullptr, handle}});
        lock.unlock();

        return load(id, handle);
    }

    uint32_t VKTextureManager::get(AssetID id)
    {
        return textureIds.at(id).bindlessId;
    }

    bool VKTextureManager::isLoaded(AssetID id)
    {
        return textureIds.contains(id);
    }

    void VKTextureManager::unload(AssetID id)
    {
        std::lock_guard lock{idMutex};
        TexInfo info = textureIds[id];
        if (info.bindlessId == missingTextureID)
        {
            // Unloading the missing texture will break a lot of things so... let's just not
            return;
        }

        if (info.tex != missingTexture)
        {
            delete info.tex;
        }

        textureIds.erase(id);
        textureManager->FreeTextureHandle(info.bindlessId);
    }

    void VKTextureManager::release(AssetID id)
    {
        TexInfo& info = textureIds.at(id);
        info.refCount--;

        if (info.refCount <= 0) unload(id);
    }

    uint32_t VKTextureManager::load(AssetID id, uint32_t handle)
    {
        TextureData td = loadTexData(id);

        if (td.data == nullptr)
        {
            textureManager->SetTextureAt(handle, missingTexture);
            TexInfo& ti = textureIds[id];
            ti.tex = missingTexture;
            ti.refCount = 1;
            return handle;
        }

        R2::VK::TextureCreateInfo tci = R2::VK::TextureCreateInfo::Texture2D(td.format, td.width, td.height);

        tci.NumMips = td.numMips;

        if (td.isCubemap)
        {
            tci.Dimension = R2::VK::TextureDimension::Cube;
            tci.Layers = 1;
            tci.NumMips = 5; // Give cubemaps 5 mips for convolution
        }

        R2::VK::Texture* t = core->CreateTexture(tci);
        t->SetDebugName(td.name.c_str());
        TexInfo& ti = textureIds[id];
        ti.tex = t;
        ti.refCount = 1;
        textureManager->SetTextureAt(handle, t);

        if (!td.isCubemap)
            core->QueueTextureUpload(t, td.data, td.totalDataSize);
        else
            core->QueueTextureUpload(t, td.data, td.totalDataSize, 1);

        free(td.data);

        return handle;
    }

    void VKTextureManager::showDebugMenu()
    {
        if (ImGui::Begin("Texture Manager"))
        {
            ImGui::Text("%zu textures loaded", textureIds.size());

            for (auto& p : textureIds)
            {
                ImGui::Text("%s refcount %i", AssetDB::idToPath(p.first).c_str(), p.second.refCount);
            }
        }
        ImGui::End();
    }
}