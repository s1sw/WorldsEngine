#pragma once
#include <Core/AssetDB.hpp>
#include <Core/ISystem.hpp>
#include <slib/StaticAllocList.hpp>
#include <Libs/pcg_basic.h>

namespace lg {
    class PlayerSoundSystem : public worlds::ISystem {
    public:
        PlayerSoundSystem(worlds::EngineInterfaces interfaces, entt::registry& registry);
        void update(entt::registry& registry, float deltaTime, float interpAlpha) override;
    private:
        worlds::EngineInterfaces interfaces;
        worlds::AssetID jumpSound;
        worlds::AssetID landSound;
        worlds::AssetID wallJumpSound;
        slib::StaticAllocList<worlds::AssetID> doubleJumpSounds;
        slib::StaticAllocList<worlds::AssetID> footstepSounds;
        pcg32_random_t rng;
    };
}
