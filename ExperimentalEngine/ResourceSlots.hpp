#pragma once
#include <unordered_map>
#include "Fatal.hpp"
#include "TextureLoader.hpp"
#include "CubemapLoader.hpp"
#include "PackedMaterial.hpp"
#include <array>

namespace worlds {
    const uint32_t NUM_TEX_SLOTS = 64;
    const uint32_t NUM_MAT_SLOTS = 256;
    const uint32_t NUM_CUBEMAP_SLOTS = 64;
    struct VulkanCtx;

    template <typename slotType, uint32_t slotCount, typename key>
    class ResourceSlots {
    protected:
        std::array<slotType, slotCount> slots;
        std::array<bool, slotCount> present;
        std::unordered_map<key, uint32_t> lookup;
        std::unordered_map<uint32_t, key> reverseLookup;
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
        ResourceSlots() : present(), slots() {

        }

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
        virtual void unload(int idx) = 0;
    };

    class TextureSlots : public ResourceSlots<vku::TextureImage2D, NUM_TEX_SLOTS, AssetID> {
    protected:
        uint32_t load(AssetID asset) override {
            uint32_t slot = getFreeSlot();

            if (slot > NUM_TEX_SLOTS) {
                fatalErr("Out of texture slots");
            }

            present[slot] = true;

            auto texData = loadTexData(asset);
            if (cb != VK_NULL_HANDLE)
                slots[slot] = uploadTextureVk(*vkCtx, texData, cb, imageIndex);
            else
                slots[slot] = uploadTextureVk(*vkCtx, texData);
            std::free(texData.data);

            lookup.insert({ asset, slot });
            reverseLookup.insert({ slot, asset });
            return slot;
        }
    private:
        std::shared_ptr<VulkanCtx> vkCtx;
        vk::CommandBuffer cb;
        uint32_t imageIndex;
    public:
        TextureSlots(std::shared_ptr<VulkanCtx> vkCtx) : vkCtx(vkCtx), cb(nullptr) {

        }

        void setUploadCommandBuffer(vk::CommandBuffer cb, uint32_t imageIndex) {
            this->cb = cb;
            this->imageIndex = imageIndex;
        }

        void unload(int idx) {
            present[idx] = false;
            slots[idx] = vku::TextureImage2D{};
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
        std::shared_ptr<VulkanCtx> vkCtx;
        std::array<MatExtraData, NUM_MAT_SLOTS> matExtraData;
        TextureSlots& texSlots;
    public:
        MatExtraData& getExtraDat(uint32_t slot) { assert(this->present[slot]); return matExtraData[slot]; }

        MaterialSlots(std::shared_ptr<VulkanCtx> vkCtx, TextureSlots& texSlots) : vkCtx(vkCtx), texSlots(texSlots) {

        }

        void unload(int idx) {
            present[idx] = false;
            lookup.erase(reverseLookup.at(idx));
            reverseLookup.erase(idx);
        }
    };

    class CubemapSlots : public ResourceSlots<vku::TextureImageCube, NUM_CUBEMAP_SLOTS, AssetID> {
    protected:
        uint32_t load(AssetID asset) override {
            uint32_t slot = getFreeSlot();

            if (slot > NUM_CUBEMAP_SLOTS) {
                fatalErr("Out of cubemap slots");
            }

            present[slot] = true;

            auto cubemapData = loadCubemapData(asset);
            if (cb == VK_NULL_HANDLE)
                slots[slot] = uploadCubemapVk(*vkCtx, cubemapData);
            else
                slots[slot] = uploadCubemapVk(*vkCtx, cubemapData, cb, imageIndex);

            lookup.insert({ asset, slot });
            reverseLookup.insert({ slot, asset });

            return slot;
        }
    private:
        std::shared_ptr<VulkanCtx> vkCtx;
        vk::CommandBuffer cb;
        uint32_t imageIndex;
    public:
        void setUploadCommandBuffer(vk::CommandBuffer cb, uint32_t imageIndex) {
            this->cb = cb;
            this->imageIndex = imageIndex;
        }

        CubemapSlots(std::shared_ptr<VulkanCtx> vkCtx) : vkCtx(vkCtx) {

        }

        void unload(int idx) override {
            present[idx] = false;
            slots[idx] = vku::TextureImageCube{};
            lookup.erase(reverseLookup.at(idx));
            reverseLookup.erase(idx);
        }
    };
}