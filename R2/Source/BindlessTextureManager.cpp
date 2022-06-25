#include <R2/BindlessTextureManager.hpp>

namespace R2
{
    BindlessTextureManager::BindlessTextureManager(const VK::Handles* handles)
        : handles(handles)
    {
    }

    BindlessTextureManager::~BindlessTextureManager()
    {

    }

    uint32_t BindlessTextureManager::AllocateTextureHandle()
    {
        if (freeLocation == ~0u)
        {
            textures.push_back(nullptr);
            return textures.size() - 1;
        }
        else
        {
            // follow the chain!!

            uint32_t freeSlot = freeLocation;
            VK::Texture* slotValue = textures[freeSlot];

            // if the freeLocation is the only free slot, just use that
            // handled separately because we have to reset freeLocation
            if (slotValue == nullptr)
            {
                freeLocation = ~0u;
                return freeSlot;
            }

            uint32_t lastSlot = ~0u;

            // otherwise follow the chain of slots to find the final free slot
            // (will have a null pointer)
            while (slotValue != nullptr)
            {
                lastSlot = freeSlot;
                freeSlot = reinterpret_cast<uintptr_t>(slotValue);
                slotValue = textures[freeSlot];
            }

            textures[lastSlot] = nullptr;

            return freeSlot;
        }
    }

    void BindlessTextureManager::SetTextureAt(uint32_t handle, VK::Texture* tex)
    {
        textures[handle] = tex;
    }

    void BindlessTextureManager::FreeTextureHandle(uint32_t handle)
    {
        if (freeLocation == ~0u)
        {
            freeLocation = handle;
            textures[handle] = nullptr;
            return;
        }

        uint32_t freeSlot = freeLocation;
        VK::Texture* slotValue = textures[freeSlot];

        while (slotValue != nullptr)
        {
            freeSlot = reinterpret_cast<uintptr_t>(slotValue);
            slotValue = textures[freeSlot];
        }

        textures[freeSlot] = reinterpret_cast<VK::Texture*>(handle);
    }
}