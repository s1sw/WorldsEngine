#include "DroneAI.hpp"
#include "LocospherePlayerSystem.hpp"
#include "MathsUtil.hpp"
#include <Physics/PhysicsActor.hpp>
#include <Audio/Audio.hpp>
#include <Core/AssetDB.hpp>
#include <Serialization/SceneSerialization.hpp>

namespace lg {
    DroneAISystem::DroneAISystem(worlds::EngineInterfaces interfaces, entt::registry& registry) {
    }

    void DroneAISystem::onSceneStart(entt::registry& registry) {
        registry.view<DroneAI, Transform>().each([](DroneAI& ai, Transform& t) {
            ai.currentTarget = t.position;
        });
    }

    void DroneAISystem::simulate(entt::registry& registry, float simStep) {
        auto view = registry.view<DroneAI, worlds::DynamicPhysicsActor>();
        auto locosphereView = registry.view<LocospherePlayerComponent>();
        if (locosphereView.size() == 0) return;
        entt::entity player = locosphereView[0];
        Transform& playerTransform = registry.get<Transform>(player);

        const float burstPeriod = 3.0f;
        view.each([&](DroneAI& ai, worlds::DynamicPhysicsActor& dpa) {
            const Transform& pose = dpa.pose();
            ai.timeSinceLastShot += simStep;
            ai.timeSinceLastBurst += simStep;

            // Find player target if we're not about to fire a burst
            if (!ai.firingBurst)
                ai.currentTarget = playerTransform.position + glm::vec3(0.0f, 1.0f, 0.0f);

            worlds::RaycastHitInfo rhi;
            if (!worlds::raycast(pose.position - glm::vec3(0.0f, 0.5f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), 5.0f, &rhi)) return;

            glm::vec3 target = ai.currentTarget;
            glm::vec3 playerDirection = target - pose.position;
            playerDirection.y = glm::clamp(playerDirection.y, -2.0f, 2.0f);
            playerDirection = glm::normalize(playerDirection);

            target -= playerDirection * 3.5f;
            target.y = glm::max(target.y + 0.5f, rhi.worldPos.y + 2.5f);

            glm::quat targetRotation = safeQuatLookat(-playerDirection);
            glm::vec3 actualDirection = pose.transformDirection(glm::vec3(0.0f, 0.0f, 1.0f));

            // Apply positional PD
            glm::vec3 force = ai.pd.getOutput(pose.position, target, dpa.linearVelocity(), simStep);
            force = glm::clamp(force, ai.minPositionalForces, ai.maxPositionalForces);
            dpa.addForce(force, worlds::ForceMode::Force);

            // Assume a target identity rotation for now
            glm::quat quatDiff = targetRotation * glm::inverse(fixupQuat(pose.rotation));
            quatDiff = fixupQuat(quatDiff);

            float angle = glm::angle(quatDiff);
            glm::vec3 axis = glm::axis(quatDiff);
            angle = glm::degrees(angle);
            angle = AngleToErr(angle);
            angle = glm::radians(angle);

            glm::vec3 torque = ai.rotationPID.getOutput(angle * axis, simStep);
            float torqueScalar = glm::clamp((burstPeriod - ai.timeSinceLastBurst), 0.0f, 1.0f);

            if (glm::dot(actualDirection, playerDirection) < 0.95f) torqueScalar = 1.0f;
            dpa.addTorque(torque * glm::pow(torqueScalar, 0.5f));

            if (glm::dot(actualDirection, playerDirection) > 0.95f) {
                static bool playedSoundThisBurst = false;
                if (ai.timeSinceLastBurst > burstPeriod - 1.0f && !playedSoundThisBurst) {
                    worlds::AudioSystem::getInstance()->playOneShotClip(
                        worlds::AssetDB::pathToId("Audio/SFX/drone pre burst.ogg"), pose.position, true
                    );
                    playedSoundThisBurst = true;
                }

                if (ai.timeSinceLastBurst > 3.0f) {
                    ai.firingBurst = true;
                    ai.timeSinceLastBurst = 0.0f;
                    playedSoundThisBurst = false;
                }
            }

            if (ai.firingBurst && ai.timeSinceLastBurst > 0.5f)
                ai.firingBurst = false;

            if (ai.firingBurst && ai.timeSinceLastShot > 0.1f) {
                ai.timeSinceLastShot = 0.0f;

                worlds::AssetID projectileId = worlds::AssetDB::pathToId("Prefabs/gun_projectile.wprefab");
                entt::entity projectile = worlds::SceneLoader::createPrefab(projectileId, registry);

                Transform& projectileTransform = registry.get<Transform>(projectile);
                Transform firePointTransform = ai.firePoint.transformBy(pose);

                glm::vec3 forward = firePointTransform.rotation * glm::vec3(0.0f, 0.0f, 1.0f);

                projectileTransform.position = firePointTransform.position;
                projectileTransform.rotation = firePointTransform.rotation;

                worlds::AudioSystem::getInstance()->playOneShotClip(
                    worlds::AssetDB::pathToId("Audio/SFX/gunshot.ogg"), projectileTransform.position, true
                );

                worlds::DynamicPhysicsActor& projectileDpa = registry.get<worlds::DynamicPhysicsActor>(projectile);

                // Recoil is disabled until we get a version of EnTT with reference stability
                //dpa.addForce(-forward * 100.0f * projectileDpa.mass, worlds::ForceMode::Impulse);

                projectileDpa.actor->setGlobalPose(worlds::glm2px(projectileTransform));
                projectileDpa.addForce(forward * 100.0f, worlds::ForceMode::VelocityChange);
            }
        });
    }
}
