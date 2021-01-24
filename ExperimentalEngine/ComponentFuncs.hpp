#pragma once
#include <entt/entity/fwd.hpp>
#include <physfs.h>

namespace worlds {
    struct ComponentEditorLink;

    class ComponentEditor {
    public:
        static ComponentEditorLink* first;
        ComponentEditor();
        virtual int getSortID() { return 0; }
        virtual const char* getName() = 0;
        virtual uint32_t getSerializedID() = 0;
        virtual bool allowInspectorAdd() = 0;
        virtual ENTT_ID_TYPE getComponentID() = 0;
        virtual void create(entt::entity ent, entt::registry& reg) = 0;
        virtual void clone(entt::entity from, entt::entity to, entt::registry& reg) = 0;
        virtual void edit(entt::entity ent, entt::registry& reg) = 0;
        virtual void writeToFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file) = 0;
        virtual void readFromFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file, int version) = 0;
    };

    struct ComponentEditorLink {
        ComponentEditorLink() : editor(nullptr), next(nullptr) {}
        ComponentEditor* editor;
        ComponentEditorLink* next;
    };
}