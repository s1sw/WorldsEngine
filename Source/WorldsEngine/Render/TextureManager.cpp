#include <Core/Fatal.hpp>
#include <Core/TaskScheduler.hpp>
#include <R2/BindlessTextureManager.hpp>
#include <R2/VKCore.hpp>
#include <Render/Loaders/TextureLoader.hpp>
#include <Render/RenderInternal.hpp>
#include <ImGui/imgui.h>
#include <Core/Log.hpp>

namespace worlds
{
    struct VKTextureManager::TextureLoadTask : enki::ITaskSet
    {
        AssetID id;
        uint32_t handle;
        VKTextureManager* vkTextureManager;

        TextureLoadTask(AssetID id, uint32_t handle, VKTextureManager* vkTextureManager)
            : id(id)
              , handle(handle)
              , vkTextureManager(vkTextureManager)
        {
        }

        void ExecuteRange(enki::TaskSetPartition range, uint32_t threadnum) override
        {
            R2::BindlessTextureManager* textureManager = vkTextureManager->textureManager;
            TextureData td = loadTexData(id);

            if (td.data == nullptr)
            {
                textureManager->SetTextureAt(handle, vkTextureManager->missingTexture);
                TexInfo& ti = vkTextureManager->textureIds[id];
                ti.tex = vkTextureManager->missingTexture;
                ti.refCount = 1;
                return;
            }

            R2::VK::TextureCreateInfo tci = R2::VK::TextureCreateInfo::Texture2D(td.format, td.width, td.height);

            tci.NumMips = td.numMips;

            if (td.isCubemap)
            {
                tci.Dimension = R2::VK::TextureDimension::Cube;
                tci.Layers = 1;
                tci.NumMips = 5; // Give cubemaps 5 mips for convolution
            }

            R2::VK::Texture* t = vkTextureManager->core->CreateTexture(tci);
            t->SetDebugName(td.name.c_str());
            TexInfo& ti = vkTextureManager->textureIds[id];
            ti.tex = t;
            ti.refCount = 1;
            ti.isCubemap = td.isCubemap;


            if (!td.isCubemap)
                vkTextureManager->core->QueueTextureUpload(t, td.data, td.totalDataSize);
            else
                vkTextureManager->core->QueueTextureUpload(t, td.data, td.totalDataSize, 1);

            free(td.data);
            textureManager->SetTextureAt(handle, t);
        }
    };

    VKUITextureManager::VKUITextureManager(VKTextureManager* texMan) : texMan(texMan)
    {
    }

    ImTextureID VKUITextureManager::loadOrGet(AssetID id)
    {
        return (ImTextureID)(uint64_t)texMan->loadAndGetAsync(id);
    }

    void VKUITextureManager::unload(AssetID id)
    {
        texMan->unload(id);
    }

    VKTextureManager::VKTextureManager(R2::VK::Core* core, R2::BindlessTextureManager* textureManager)
        : core(core), textureManager(textureManager)
    {
        missingTextureID = loadSynchronous(AssetDB::pathToId("Textures/missing.wtex"));
        missingTexture = textureManager->GetTextureAt(missingTextureID);
        AssetDB::registerAssetChangeCallback(
            [&](AssetID asset)
            {
                if (!textureIds.contains(asset)) return;
                logMsg("Reloading %s", AssetDB::idToPath(asset).c_str());
                auto& texInfo = textureIds.at(asset);
                if (texInfo.isCubemap)
                {
                    // Fully unload cubemaps so convolution can take care of them
                    unload(asset);
                    return;
                }
                if (texInfo.tex != missingTexture)
                {
                    delete texInfo.tex;
                }
                load(asset, texInfo.bindlessId);
            }
        );
    }

    VKTextureManager::~VKTextureManager()
    {
        for (auto& pair : textureIds)
        {
            if (pair.second.tex != missingTexture)
                delete pair.second.tex;
            textureManager->FreeTextureHandle(pair.second.bindlessId);
        }
        delete missingTexture;
    }

    uint32_t VKTextureManager::loadAndGetAsync(AssetID id)
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
        uint32_t handle = textureManager->AllocateTextureHandle(missingTexture);
        textureIds.insert({id, TexInfo{nullptr, handle}});
        lock.unlock();

        auto tlt = new TextureLoadTask{id, handle, this};
        TaskDeleter* td = new TaskDeleter();
        td->SetDependency(td->dependency, tlt);
        td->deleteSelf = true;
        g_taskSched.AddTaskSetToPipe(tlt);
        return handle;
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

        if (info.refCount <= 0)
            unload(id);
    }


    uint32_t VKTextureManager::load(AssetID id, uint32_t handle)
    {
        TextureLoadTask tlt{id, handle, this};
        g_taskSched.AddTaskSetToPipe(&tlt);
        g_taskSched.WaitforTask(&tlt);
        return handle;
    }

    enki::ITaskSet* VKTextureManager::loadAsync(AssetID id)
    {
        std::unique_lock lock{idMutex};
        assert(!textureIds.contains(id));
        uint32_t handle = textureManager->AllocateTextureHandle(missingTexture);
        textureIds.insert({id, TexInfo{nullptr, handle}});
        lock.unlock();
        auto tlt = new TextureLoadTask{id, handle, this};
        return tlt;
    }

    uint32_t VKTextureManager::loadSynchronous(worlds::AssetID id)
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
        uint32_t handle = textureManager->AllocateTextureHandle(missingTexture);
        textureIds.insert({id, TexInfo{nullptr, handle}});
        lock.unlock();

        TextureLoadTask tlt{id, handle, this};
        g_taskSched.AddTaskSetToPipe(&tlt);
        g_taskSched.WaitforTask(&tlt);
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
