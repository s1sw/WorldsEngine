#include "StabbySystem.hpp"
#include "Core/Log.hpp"
#include "Physics/D6Joint.hpp"
#include "Physics/Physics.hpp"
#include "Stabby.hpp"
#include <ImGui/imgui.h>
#include <Audio/Audio.hpp>
#include <queue>

namespace lg {
    struct Stab {
        entt::entity ent;
        worlds::PhysicsContactInfo info;
    };
    std::queue<Stab> stabs;

    StabbySystem::StabbySystem(worlds::EngineInterfaces interfaces, entt::registry& registry) {
        registry.on_construct<Stabby>().connect<&StabbySystem::onStabbyConstruct>(*this);
    }

    void StabbySystem::simulate(entt::registry& registry, float simStep) {
        while (!stabs.empty()) {
            Stab stab = stabs.front();
            stabs.pop();

            entt::entity ent = stab.ent;
            worlds::PhysicsContactInfo info = stab.info;

            Stabbable* stabbable = registry.try_get<Stabbable>(info.otherEntity);
            if (!stabbable) continue;

            Stabby& stabby = registry.get<Stabby>(ent);
            Transform& t = registry.get<Transform>(ent);
            Transform& otherTransform = registry.get<Transform>(info.otherEntity);
            worlds::DynamicPhysicsActor& stabbyDpa = registry.get<worlds::DynamicPhysicsActor>(ent);

            // Get alignment between stab direction and surface
            // If they're too different, don't stab
            glm::vec3 stabDir = t.transformDirection(stabby.stabDirection);
            float dot = glm::abs(glm::dot(info.normal, stabDir));

            if (dot < 0.707f || registry.has<worlds::D6Joint>(ent)) continue;

            worlds::D6Joint& d6 = registry.emplace<worlds::D6Joint>(ent);
            d6.setTarget(info.otherEntity, registry);

            Transform relativeTransform = t.transformByInverse(otherTransform);
            d6.pxJoint->setLocalPose(physx::PxJointActorIndex::eACTOR1, worlds::glm2px(relativeTransform));


            d6.setAllAngularMotion(worlds::D6Motion::Locked);
            d6.setAllLinearMotion(worlds::D6Motion::Locked);
            d6.pxJoint->setMotion(physx::PxD6Axis::eY, physx::PxD6Motion::eLIMITED);
            physx::PxJointLinearLimitPair linLimit{physx::PxTolerancesScale{}, -stabby.pulloutDistance, 0.3f};
            d6.pxJoint->setLinearLimit(physx::PxD6Axis::eY, linLimit);

            stabby.entryPoint = otherTransform.inverseTransformPoint(t.position);
            logMsg("stab");
            stabby.embedded = true;
            stabby.embeddedIn = info.otherEntity;

            worlds::AssetID soundClip = stabbable->stabSound == ~0u ? worlds::AssetDB::pathToId("Audio/SFX/stabby.ogg") : stabbable->stabSound;

            worlds::AudioSystem::getInstance()->playOneShotClip(
                soundClip,
                info.averageContactPoint, true
            );

            stabbyDpa.actor->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);
            stabbyDpa.actor->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, false);
        }

        auto view = registry.view<Stabby, Transform, worlds::DynamicPhysicsActor>();
        view.each([&](entt::entity ent, Stabby& stabby, Transform& t, worlds::DynamicPhysicsActor& dpa) {
            if (!stabby.embedded) return;

            if (!registry.valid(stabby.embeddedIn)) {
                stabby.embedded = false;
                return;
            }

            Transform& stabbedTransform = registry.get<Transform>(stabby.embeddedIn);

            glm::vec3 movedDir = stabbedTransform.transformPoint(stabby.entryPoint) - t.position;
            movedDir = t.inverseTransformDirection(movedDir);

            glm::vec3 velocity = dpa.linearVelocity();
            glm::vec3 wsStabDir = t.transformDirection(stabby.stabDirection);

            float clampedDot = glm::clamp(glm::dot(glm::normalize(velocity), wsStabDir), stabby.dragFloor, 1.0f);
            dpa.addForce(-velocity * stabby.dragMultiplier * clampedDot, worlds::ForceMode::Acceleration);

            float distance = glm::dot(movedDir, stabby.stabDirection);
            if (distance > 0.2f) {
                registry.remove<worlds::D6Joint>(ent);
                stabby.embedded = false;
            }
        });
    }

    void StabbySystem::onStabbyConstruct(entt::registry& registry, entt::entity ent) {
        worlds::PhysicsEvents& physEvts = registry.get_or_emplace<worlds::PhysicsEvents>(ent);

        physEvts.addContactCallback([&](entt::entity ent, const worlds::PhysicsContactInfo& info) {
            stabs.push(Stab { ent, info });
        });
    }
}
