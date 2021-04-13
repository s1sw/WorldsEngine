#include "ResourceSlots.hpp"
#include "Render.hpp"

namespace worlds {
    CubemapSlots::CubemapSlots(std::shared_ptr<VulkanHandles> vkCtx, std::shared_ptr<CubemapConvoluter> cc)
        : vkCtx(vkCtx)
        , imageIndex(0)
        , cc{ cc }{

    }

    uint32_t CubemapSlots::load(AssetID asset) {
        uint32_t slot = getFreeSlot();

        if (slot > NUM_CUBEMAP_SLOTS) {
            fatalErr("Out of cubemap slots");
        }


        if (!PHYSFS_exists(g_assetDB.getAssetPath(asset).c_str())) {
            uint32_t missingSlot = loadOrGet(g_assetDB.addOrGetExisting("Cubemaps/missing.json"));
            lookup.insert({ asset, missingSlot });
            return missingSlot;
        }

        present[slot] = true;

        auto cubemapData = loadCubemapData(asset);
        //if (!cb)
        //else
        //    slots[slot] = uploadCubemapVk(*vkCtx, cubemapData, cb, imageIndex);

        slots[slot] = uploadCubemapVk(*vkCtx, cubemapData);
        cc->convolute(slots[slot]);

        lookup.insert({ asset, slot });
        reverseLookup.insert({ slot, asset });

        return slot;
    }

    void CubemapSlots::unload(int idx) {
        present[idx] = false;
        slots[idx] = vku::TextureImageCube{};
        lookup.erase(reverseLookup.at(idx));
        reverseLookup.erase(idx);
    }
}
