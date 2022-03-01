#include "ResourceSlots.hpp"
#include <mutex>

namespace worlds {
    uint32_t TextureSlots::load(AssetID asset) {
        slotMutex->lock();
        uint32_t slot = getFreeSlot();

        if (slot > NUM_TEX_SLOTS) {
            fatalErr("Out of texture slots");
        }

        present[slot] = true;
        lookup.insert({ asset, slot });
        reverseLookup.insert({ slot, asset });
        slotMutex->unlock();

        auto texData = loadTexData(asset);
        if (texData.data == nullptr) {
            texData = loadTexData(AssetDB::pathToId("Textures/missing.wtex"));
        }

        if (cb && frameStarted)
            slots[slot] = uploadTextureVk(*vkCtx, texData, cb, frameIdx);
        else
            slots[slot] = uploadTextureVk(*vkCtx, texData);
        std::free(texData.data);

        return slot;
    }

    TextureSlots::TextureSlots(std::shared_ptr<VulkanHandles> vkCtx)
        : vkCtx(vkCtx)
        , cb(nullptr) {
        slotMutex = new std::mutex;
    }

    void TextureSlots::setUploadCommandBuffer(VkCommandBuffer cb, uint32_t frameIdx) {
        this->cb = cb;
        this->frameIdx = frameIdx;
    }

    void TextureSlots::unload(uint32_t idx) {
        present[idx] = false;
        slots[idx].destroy();
        lookup.erase(reverseLookup.at(idx));
        reverseLookup.erase(idx);
    }

    TextureSlots::~TextureSlots() {
        delete slotMutex;
    }
}
