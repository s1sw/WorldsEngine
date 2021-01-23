#pragma once
#include <entt/entity/fwd.hpp>

namespace worlds {
    struct ComponentEditorLink;

    class ComponentEditor {
    public:
        static ComponentEditorLink* first;
        ComponentEditor();
        virtual const char* getName() = 0;
        virtual bool allowInspectorAdd() = 0;
        virtual ENTT_ID_TYPE getComponentID() = 0;
        virtual void create(entt::entity ent, entt::registry& reg) = 0;
        virtual void clone(entt::entity from, entt::entity to, entt::registry& reg) = 0;
        virtual void edit(entt::entity ent, entt::registry& reg) = 0;
    };

    struct ComponentEditorLink {
        ComponentEditorLink() : editor(nullptr), next(nullptr) {}
        ComponentEditor* editor;
        ComponentEditorLink* next;
    };

    void createLight(entt::entity ent, entt::registry& reg);
    void editLight(entt::entity ent, entt::registry& reg);
    void cloneLight(entt::entity from, entt::entity to, entt::registry& reg);

    void createPhysicsActor(entt::entity ent, entt::registry& reg);
    void editPhysicsActor(entt::entity ent, entt::registry& reg);
    void clonePhysicsActor(entt::entity a, entt::entity b, entt::registry& reg);

    void createDynamicPhysicsActor(entt::entity ent, entt::registry& reg);
    void editDynamicPhysicsActor(entt::entity ent, entt::registry& reg);
    void cloneDynamicPhysicsActor(entt::entity a, entt::entity b, entt::registry& reg);

    void editNameComponent(entt::entity ent, entt::registry& registry);
    void createNameComponent(entt::entity ent, entt::registry& registry);
    void cloneNameComponent(entt::entity a, entt::entity b, entt::registry& reg);

    void editAudioSource(entt::entity ent, entt::registry& registry);
    void createAudioSource(entt::entity ent, entt::registry& reg);
    void cloneAudioSource(entt::entity a, entt::entity b, entt::registry& reg);

    void createWorldCubemap(entt::entity ent, entt::registry& reg);
    void editWorldCubemap(entt::entity ent, entt::registry& reg);

    void createD6Joint(entt::entity ent, entt::registry& reg);
    void editD6Joint(entt::entity ent, entt::registry& reg);
}