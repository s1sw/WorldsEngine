#include <ComponentMeta/ComponentMetadata.hpp>

namespace worlds
{
    std::unordered_map<ENTT_ID_TYPE, ComponentMetadata*> ComponentMetadataManager::metadata;
    std::vector<ComponentMetadata*> ComponentMetadataManager::sorted;
    std::unordered_map<ENTT_ID_TYPE, ComponentMetadata*> ComponentMetadataManager::bySerializedID;
    std::unordered_map<std::string, ComponentMetadata*> ComponentMetadataManager::byName;
}