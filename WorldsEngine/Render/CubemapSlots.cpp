#include "ResourceSlots.hpp"
#include "RenderInternal.hpp"

namespace worlds {
    CubemapSlots::CubemapSlots(std::shared_ptr<VulkanHandles> vkCtx, std::shared_ptr<CubemapConvoluter> cc)
        : vkCtx(vkCtx)
        , imageIndex(0)
        , cc{ cc }{
        missingSlot = loadOrGet(AssetDB::pathToId("Cubemaps/missing.json"));
    }

    uint32_t CubemapSlots::loadOrGet(AssetID id) {
        auto iter = lookup.find(id);

        if (iter != lookup.end() && iter->second != missingSlot) return iter->second;

        return load(id);
    }

    uint32_t CubemapSlots::load(AssetID asset) {
        uint32_t slot = getFreeSlot();

        if (slot > NUM_CUBEMAP_SLOTS) {
            fatalErr("Out of cubemap slots");
        }

        if (!PHYSFS_exists(AssetDB::idToPath(asset).c_str())) {
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
        if (idx == missingSlot) return;

        present[idx] = false;
        slots[idx].destroy();
        lookup.erase(reverseLookup.at(idx));
        reverseLookup.erase(idx);
    }
}
