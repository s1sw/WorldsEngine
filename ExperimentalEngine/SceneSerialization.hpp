#pragma once
#include <cstdint>
#include <entt/entt.hpp>

typedef uint32_t AssetID;
void saveScene(AssetID id, entt::registry& reg);
void loadScene(AssetID id, entt::registry& reg);