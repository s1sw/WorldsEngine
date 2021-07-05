#include <Audio/Audio.hpp>
#include <Input/Input.hpp>
#include <VR/IVRInterface.hpp>
#include "PlayerSoundSystem.hpp"
#include "LocospherePlayerSystem.hpp"
#include "PlayerSoundComponent.hpp"

namespace lg {
    PlayerSoundSystem::PlayerSoundSystem(worlds::EngineInterfaces interfaces, entt::registry& registry)
        : interfaces(interfaces)
        , doubleJumpSounds(3)
        , footstepSounds(10) {
        for (int i = 1; i <= 10; i++) {
            std::string path = "Audio/SFX/Footsteps/Concrete/step" +
                std::string(i < 10 ? "0" : "") + std::to_string(i) + ".ogg";
            footstepSounds.add(worlds::AssetDB::pathToId(path));
        }

        for (int i = 1; i <= 3; i++) {
            std::string path = "Audio/SFX/Player/double_jump" +
                std::to_string(i) + ".ogg";
            doubleJumpSounds.add(worlds::AssetDB::pathToId(path));
        }

        jumpSound = worlds::AssetDB::pathToId("Audio/SFX/Player/jump.ogg");
        landSound = worlds::AssetDB::pathToId("Audio/SFX/Footsteps/Concrete/land.ogg");

        wallJumpSound = worlds::AssetDB::pathToId("Audio/SFX/Player/wall_jump.ogg");

        // seed the RNG
        // these numbers don't really matter
        pcg32_srandom_r(&rng, 135u, 3151u);
    }

    void PlayerSoundSystem::update(entt::registry& registry, float deltaTime, float interpAlpha) {
        auto view = registry.view<PlayerSoundComponent, LocospherePlayerComponent, Transform>();

        worlds::AudioSystem* audioSystem = worlds::AudioSystem::getInstance();
        view.each([&](PlayerSoundComponent& psc, LocospherePlayerComponent& lpc, Transform& transform) {
            psc.timeSinceLastJump += deltaTime;

            if (lpc.jump && psc.timeSinceLastJump > 0.2f) {
                psc.timeSinceLastJump = 0.0f;
                if (lpc.grounded) {
                    audioSystem->playOneShotClip(
                            jumpSound, transform.position, false, 0.6f);
                } else if (lpc.canWallJump) {
                    audioSystem->playOneShotClip(
                            wallJumpSound, transform.position, false, 0.6f);
                }
            }

            if (!psc.dblJumpUsedLast && lpc.doubleJumpUsed) {
                static int lastSoundIdx = 0;
                int soundIdx = 0;
                while (lastSoundIdx == soundIdx) soundIdx = pcg32_boundedrand_r(&rng, doubleJumpSounds.numElements());
                lastSoundIdx = soundIdx;
                audioSystem->playOneShotClip(
                        doubleJumpSounds[soundIdx], transform.position, false, 0.25f);
            }

            if (!psc.groundedLast && lpc.grounded) {
                worlds::AudioSystem::getInstance()->playOneShotClip(
                        landSound, transform.position, false, 0.5f);
            }

            psc.dblJumpUsedLast = lpc.doubleJumpUsed;
            psc.groundedLast = lpc.grounded;

            if (lpc.grounded) {
                float inputMagnitude = glm::length(lpc.xzMoveInput);
                inputMagnitude *= lpc.sprint ? 1.5f : 1.0f;
                psc.stepTimer += inputMagnitude * deltaTime * 2.0f;
            }

            if (psc.stepTimer >= 1.0f) {
                // make sure we don't repeat either the last sound or the sound before that
                static int lastSoundIdx = 0;
                static int lastLastSoundIdx = 0;
                int soundIdx = 0;

                // just keep generating numbers until we get an index
                // meeting that criteria
                while (lastSoundIdx == soundIdx || soundIdx == lastLastSoundIdx)
                    soundIdx = pcg32_boundedrand_r(&rng, footstepSounds.numElements());

                lastLastSoundIdx = lastSoundIdx;
                lastSoundIdx = soundIdx;

                worlds::AudioSystem::getInstance()->playOneShotClip(
                    footstepSounds[soundIdx],
                    transform.position,
                    false, 0.5f
                );

                psc.stepTimer = 0.0f;
            }
        });
    }
}
