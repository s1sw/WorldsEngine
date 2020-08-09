#pragma once
#include <unordered_map>
#include "Fatal.hpp"
#include "TextureLoader.hpp"
#include "PackedMaterial.hpp"
#include <array>

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

    uint32_t loadOrGet(key k) {
        auto iter = lookup.find(k);

        if (iter != lookup.end()) return iter->second;

        return load(k);
    }

    uint32_t get(key k) const { return lookup.at(k); }
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
        slots[slot] = uploadTextureVk(*vkCtx, texData);
        std::free(texData.data);

        lookup.insert({ asset, slot });

        return slot;
    }
private:
    std::shared_ptr<VulkanCtx> vkCtx;
public:
    TextureSlots(std::shared_ptr<VulkanCtx> vkCtx) : vkCtx(vkCtx) {

    }
};

struct PackedMaterial;

class MaterialSlots : public ResourceSlots<PackedMaterial, NUM_MAT_SLOTS, AssetID> {
protected:
    void parseMaterial(AssetID asset, PackedMaterial& mat);

    uint32_t load(AssetID asset) override;
private:
    std::shared_ptr<VulkanCtx> vkCtx;
    TextureSlots& texSlots;
public:
    MaterialSlots(std::shared_ptr<VulkanCtx> vkCtx, TextureSlots& texSlots) : vkCtx(vkCtx), texSlots(texSlots) {

    }
};

class CubemapSlots : public ResourceSlots<vku::TextureImageCube, NUM_CUBEMAP_SLOTS, AssetID> {
protected:
    uint32_t load(AssetID asset) override {
        
    }
private:
    std::shared_ptr<VulkanCtx> vkCtx;
public:
    CubemapSlots(std::shared_ptr<VulkanCtx> vkCtx) : vkCtx(vkCtx) {

    }
};