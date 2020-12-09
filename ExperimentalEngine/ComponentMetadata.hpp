#pragma once
#include <unordered_map>
#include <string>
#include <entt/entt.hpp>

namespace worlds {
    typedef void (*EditComponentFuncPtr)(entt::entity, entt::registry&);
    typedef void (*AddComponentFuncPtr)(entt::entity, entt::registry&);
    typedef void (*CloneComponentFuncPtr)(entt::entity, entt::entity, entt::registry&);

    struct ComponentMetadata {
        std::string name;
        bool showInInspector;
        ENTT_ID_TYPE typeId;
        EditComponentFuncPtr editFuncPtr;
        AddComponentFuncPtr addFuncPtr;
        CloneComponentFuncPtr cloneFuncPtr;
    };

    class ComponentMetadataManager {
    public:
        using str_hash_type = entt::hashed_string::hash_type;
        static std::unordered_map<str_hash_type, ComponentMetadata> strMdata;
        static std::unordered_map<ENTT_ID_TYPE, ComponentMetadata> metadata;

        static void registerMetadata(ENTT_ID_TYPE typeId, ComponentMetadata metadata) {
            ComponentMetadataManager::metadata.insert({ typeId, metadata });
        }

        template <typename T>
        static void registerEx(
                std::string name, 
                bool showInInspector,
                EditComponentFuncPtr editFuncPtr,
                AddComponentFuncPtr addFuncPtr,
                CloneComponentFuncPtr cloneFuncPtr) {
            registerMetadata(entt::type_info<T>::id(),
                ComponentMetadata {
                    name,
                    showInInspector,
                    entt::type_info<T>::id(),
                    editFuncPtr,
                    addFuncPtr,
                    cloneFuncPtr
            });
        }
    };
}
