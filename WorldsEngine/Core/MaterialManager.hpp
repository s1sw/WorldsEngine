#pragma once
#include <nlohmann/json.hpp>
#include <robin_hood.h>

namespace worlds
{
    typedef uint32_t AssetID;
    class MaterialManager
    {
    public:
        static nlohmann::json& loadOrGet(AssetID id);
        static void reload();
    private:
        static robin_hood::unordered_map<AssetID, nlohmann::json> mats;
    };
}