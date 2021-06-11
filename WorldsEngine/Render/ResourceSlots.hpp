#pragma once
#include <robin_hood.h>
#include "../Core/Fatal.hpp"
#include "Loaders/TextureLoader.hpp"
#include "Loaders/CubemapLoader.hpp"
#include "PackedMaterial.hpp"
#include <array>
#include <nlohmann/json_fwd.hpp>
#include <mutex>

namespace worlds {
    const uint32_t NUM_TEX_SLOTS = 256;
    const uint32_t NUM_MAT_SLOTS = 256;
    const uint32_t NUM_CUBEMAP_SLOTS = 64;
    struct VulkanHandles;
    class CubemapConvoluter;

    template <typename slotType, uint32_t slotCount, typename key>
    class ResourceSlots {
    protected:
        std::array<slotType, slotCount> slots;
        std::array<bool, slotCount> present;
        robin_hood::unordered_flat_map<key, uint32_t> lookup;
        robin_hood::unordered_flat_map<uint32_t, key> reverseLookup;
        virtual uint32_t load(key k) = 0;

        uint32_t getFreeSlot() {
            for (uint32_t i = 0; i < slotCount; i++) {
                if (!present[i]) {
                    return i;
                }
            }

            return ~0u;
        }
    public:
        ResourceSlots() : slots(), present() {
            for (uint32_t i = 0; i < slotCount; i++) {
                present[i] = false;
            }
        }

        virtual ~ResourceSlots() {}

        constexpr uint32_t size() const { return slotCount; }
        slotType* getSlots() { return slots.data(); }
        bool isSlotPresent(int idx) const { return present[idx]; }
        slotType& operator[](int idx) { return slots[idx]; }
        slotType& operator[](uint32_t idx) { return slots[idx]; }

        uint32_t loadOrGet(key k) {
            auto iter = lookup.find(k);

            if (iter != lookup.end()) return iter->second;

            return load(k);
        }

        uint32_t get(key k) const { return lookup.at(k); }
        key getKeyForSlot(uint32_t idx) const { return reverseLookup.at(idx); }
        bool isLoaded(key k) const { return lookup.find(k) != lookup.end(); }
        virtual void unload(int idx) = 0;
    };

    class TextureSlots : public ResourceSlots<vku::TextureImage2D, NUM_TEX_SLOTS, AssetID> {
    protected:
        uint32_t load(AssetID asset) override {
            slotMutex.lock();
            uint32_t slot = getFreeSlot();

            if (slot > NUM_TEX_SLOTS) {
                fatalErr("Out of texture slots");
            }

            present[slot] = true;
            lookup.insert({ asset, slot });
            reverseLookup.insert({ slot, asset });
            slotMutex.unlock();

            auto texData = loadTexData(asset);
            if (texData.data == nullptr) {
                return loadOrGet(AssetDB::pathToId("Textures/missing.wtex"));
            }

            if (cb && frameStarted)
                slots[slot] = uploadTextureVk(*vkCtx, texData, cb, frameIdx);
            else
                slots[slot] = uploadTextureVk(*vkCtx, texData);
            std::free(texData.data);

            logMsg("slot %i set to imageView %zu", slot, slots[slot].imageView());

            return slot;
        }
    private:
        std::shared_ptr<VulkanHandles> vkCtx;
        vk::CommandBuffer cb;
        uint32_t frameIdx;
        std::mutex slotMutex;
    public:
        bool frameStarted = false;
        TextureSlots(std::shared_ptr<VulkanHandles> vkCtx) : vkCtx(vkCtx), cb(nullptr) {

        }

        void setUploadCommandBuffer(vk::CommandBuffer cb, uint32_t frameIdx) {
            this->cb = cb;
            this->frameIdx = frameIdx;
        }

        void unload(int idx) override {
            present[idx] = false;
            slots[idx].destroy();
            lookup.erase(reverseLookup.at(idx));
            reverseLookup.erase(idx);
        }
    };

    struct PackedMaterial;
    struct MatExtraData {
        MatExtraData() : noCull(false), wireframe(false) {}
        bool noCull;
        bool wireframe;
    };

    class MaterialSlots : public ResourceSlots<PackedMaterial, NUM_MAT_SLOTS, AssetID> {
    protected:
        void parseMaterial(AssetID asset, PackedMaterial& mat, MatExtraData& extraDat);

        uint32_t load(AssetID asset) override;
    private:
        uint32_t getTexture(nlohmann::json& j, std::string key);
        std::shared_ptr<VulkanHandles> vkCtx;
        std::array<MatExtraData, NUM_MAT_SLOTS> matExtraData;
        TextureSlots& texSlots;
        std::mutex slotMutex;
    public:
        MatExtraData& getExtraDat(uint32_t slot) { assert(this->present[slot]); return matExtraData[slot]; }

        MaterialSlots(std::shared_ptr<VulkanHandles> vkCtx, TextureSlots& texSlots) : vkCtx(vkCtx), texSlots(texSlots) {

        }

        void unload(int idx) override {
            present[idx] = false;
            lookup.erase(reverseLookup.at(idx));
            reverseLookup.erase(idx);
        }
    };

    class CubemapSlots : public ResourceSlots<vku::TextureImageCube, NUM_CUBEMAP_SLOTS, AssetID> {
    protected:
        uint32_t load(AssetID asset) override;
    private:
        std::shared_ptr<VulkanHandles> vkCtx;
        vk::CommandBuffer cb;
        uint32_t imageIndex;
        std::shared_ptr<CubemapConvoluter> cc;
    public:
        void setUploadCommandBuffer(vk::CommandBuffer cb, uint32_t imageIndex) {
            this->cb = cb;
            this->imageIndex = imageIndex;
        }

        CubemapSlots(std::shared_ptr<VulkanHandles> vkCtx, std::shared_ptr<CubemapConvoluter> cc);

        void unload(int idx) override;
    };
}
