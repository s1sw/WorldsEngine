#include "Core/NameComponent.hpp"
#include "Core/Transform.hpp"
#include "Export.hpp"
#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <nlohmann/json.hpp>

using namespace worlds;

extern "C"
{
    EXPORT void registry_getTransform(entt::registry* registry, uint32_t entity, Transform* output)
    {
        entt::entity enttEntity = (entt::entity)entity;
        *output = registry->get<Transform>(enttEntity);
    }

    EXPORT void registry_setTransform(entt::registry* registry, uint32_t entity, Transform* output)
    {
        entt::entity enttEntity = (entt::entity)entity;
        if (registry->has<ChildComponent>(enttEntity))
        {
            ChildComponent& cc = registry->get<ChildComponent>(enttEntity);
            cc.offset = output->transformByInverse(registry->get<Transform>(cc.parent));
        }
        else
        {
            registry->get<Transform>(enttEntity) = *output;
        }
    }

    EXPORT void registry_eachTransform(entt::registry* registry, void (*callback)(uint32_t))
    {
        registry->each([&](entt::entity ent) { callback((uint32_t)ent); });
    }

    EXPORT uint32_t registry_getEntityNameLength(entt::registry* registry, uint32_t entityId)
    {
        entt::entity enttEntity = (entt::entity)entityId;
        if (!registry->has<NameComponent>(enttEntity))
        {
            return ~0u;
        }
        NameComponent& nc = registry->get<NameComponent>((entt::entity)entityId);
        return nc.name.size();
    }

    EXPORT void registry_getEntityName(entt::registry* registry, uint32_t entityId, char* buffer)
    {
        entt::entity enttEntity = (entt::entity)entityId;
        if (!registry->has<NameComponent>(enttEntity))
        {
            return;
        }
        NameComponent& nc = registry->get<NameComponent>(enttEntity);
        buffer[nc.name.size()] = 0;
        strncpy(buffer, nc.name.c_str(), nc.name.size());
    }

    EXPORT void registry_setEntityName(entt::registry* registry, entt::entity entityId, char* name)
    {
        registry->emplace_or_replace<NameComponent>(entityId, name);
    }

    EXPORT void registry_destroy(entt::registry* registry, entt::entity entity)
    {
        registry->destroy(entity);
    }

    EXPORT uint32_t registry_create(entt::registry* registry)
    {
        entt::entity ent = registry->create();
        registry->emplace<Transform>(ent);
        return entt::to_integral(ent);
    }

    EXPORT void registry_setSerializedEntityInfo(void* serializationContext, const char* key, const char* value)
    {
        nlohmann::json& entityJson = *(nlohmann::json*)serializationContext;
        nlohmann::json componentJson = nlohmann::json::parse(value);
        entityJson[key] = componentJson;
    }

    EXPORT uint32_t registry_createPrefab(entt::registry* regPtr, AssetID id)
    {
        return (uint32_t)SceneLoader::createPrefab(id, *regPtr);
    }

    EXPORT uint32_t registry_valid(entt::registry* regPtr, entt::entity entity)
    {
        return regPtr->valid(entity);
    }
}
