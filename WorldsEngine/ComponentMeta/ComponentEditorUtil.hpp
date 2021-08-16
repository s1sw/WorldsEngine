#pragma once
#include <entt/fwd.hpp>
#include <entt/core/type_info.hpp>
#include "ComponentFuncs.hpp"

namespace worlds {
    template <typename T>
    class BasicComponentUtil : public ComponentEditor {
    public:
        void create(entt::entity ent, entt::registry& reg) override {
            reg.emplace<T>(ent);
        }

        void clone(entt::entity from, entt::entity to, entt::registry& r) override {
            r.emplace<T>(to, r.get<T>(from));
        }

        bool allowInspectorAdd() override {
            return true;
        }

        ENTT_ID_TYPE getComponentID() override {
            return entt::type_id<T>().hash();
        }

        uint32_t getSerializedID() override {
            return entt::hashed_string{ getName() };
        }
    };
}
