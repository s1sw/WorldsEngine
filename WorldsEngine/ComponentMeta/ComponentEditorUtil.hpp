#pragma once
#include <entt/entt.hpp>
#include <entt/core/type_info.hpp>
#include "ComponentFuncs.hpp"

namespace worlds {
    template <typename T>
    class BasicComponentUtil : public ComponentEditor {
    private:
        template <typename = typename std::is_default_constructible<T>::type>
        void createInternal(entt::entity ent, entt::registry& reg) {
            reg.emplace<T>(ent);
        }

        template <typename = typename std::is_copy_constructible<T>::type>
        void cloneInternal(entt::entity from, entt::entity to, entt::registry& reg) {
            reg.emplace<T>(to, reg.get<T>(from));
        }
    public:
        void create(entt::entity ent, entt::registry& reg) override {
            if constexpr (std::is_default_constructible<T>::value) {
                createInternal(ent, reg);
            } else {
                assert(false);
            }
        }

        void destroy(entt::entity ent, entt::registry& reg) override {
            reg.remove_if_exists<T>(ent);
        }

        void clone(entt::entity from, entt::entity to, entt::registry& r) override {
            if constexpr (std::is_copy_constructible<T>::value && !std::is_empty<T>::value)
                cloneInternal(from, to, r);
            else {
                assert(false);
            }
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
