#pragma once
#include <unordered_map>
#include <string>
#include <entt/entt.hpp>

typedef void (*EditComponentFuncPtr)(entt::entity, entt::registry&);
typedef void (*AddComponentFuncPtr)(entt::entity, entt::registry&);

struct ComponentMetadata {
    std::string name;
    bool showInInspector;
    ENTT_ID_TYPE typeId;
    EditComponentFuncPtr editFuncPtr;
    AddComponentFuncPtr addFuncPtr;
};

class ComponentMetadataManager {
public:
    static std::unordered_map<ENTT_ID_TYPE, ComponentMetadata> metadata;
    
    static void registerMetadata(ENTT_ID_TYPE typeId, ComponentMetadata metadata) {
        ComponentMetadataManager::metadata.insert({ typeId, metadata });
    }
};