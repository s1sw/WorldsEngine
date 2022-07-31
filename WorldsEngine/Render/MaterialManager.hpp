#pragma once
#include <nlohmann/json.hpp>
#include <robin_hood.h>

namespace worlds
{
    typedef uint32_t AssetID;
    class MaterialManager
    {
    public:
        nlohmann::json& loadOrGet(AssetID id);
    private:
        robin_hood::unordered_map<AssetID, nlohmann::json> mats;
    };
}