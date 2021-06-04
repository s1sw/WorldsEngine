#pragma once
#include <entt/entt.hpp>
#include <Core/ISystem.hpp>

namespace lg {
    class StabbySystem : public worlds::ISystem {
    public:
        StabbySystem(worlds::EngineInterfaces interfaces, entt::registry& registry);
        void simulate(entt::registry& registry, float simStep) override;
    private:
        void onStabbyConstruct(entt::registry& registry, entt::entity ent);
        worlds::EngineInterfaces interfaces;
    };
}
