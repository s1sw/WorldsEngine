#include "EventHandler.hpp"
#include <Util/RichPresence.hpp>
#include <openvr.h>
#include <Core/Log.hpp>
#include <Render/Render.hpp>
#include "Core/AssetDB.hpp"
#include "DebugArrow.hpp"
#include "Physics/D6Joint.hpp"
#include "Physics/PhysicsActor.hpp"
#include <physx/PxRigidDynamic.h>
#include "Core/Transform.hpp"
#include <VR/OpenVRInterface.hpp>
#include <Physics/Physics.hpp>
#include <Core/Console.hpp>
#include <ImGui/imgui.h>
#include <Util/MatUtil.hpp>
#include <Core/Engine.hpp>
#include "Core/NameComponent.hpp"
#include "LocospherePlayerSystem.hpp"
#include "PhysHandSystem.hpp"
#include <enet/enet.h>
#include <Core/JobSystem.hpp>
#include "Util/CreateModelObject.hpp"
#include "ObjectParentSystem.hpp"
#include "extensions/PxD6Joint.h"
#ifdef DISCORD_RPC
#include <core.h>
#endif
#include "Util/VKImGUIUtil.hpp"
#include <Scripting/ScriptComponent.hpp>
#include <Physics/FilterEntities.hpp>
#include "MathsUtil.hpp"
#include "PlayerStartPoint.hpp"
#include "RPGStats.hpp"
#include "GripPoint.hpp"
#include <Input/Input.hpp>
#include <Physics/FixedJoint.hpp>
#include "PhysicsSoundComponent.hpp"
#include <Audio/Audio.hpp>
#include "PlayerGrabManager.hpp"
#include "ContactDamageDealer.hpp"

namespace lg {
    struct SyncedRB {};

    void cmdToggleVsync(void* obj, const char*) {
        auto renderer = (worlds::VKRenderer*)obj;
        renderer->setVsync(!renderer->getVsync());
    }

    EventHandler::EventHandler(bool dedicatedServer)
        : isDedicated {dedicatedServer}
        , client {nullptr}
        , server {nullptr}
        , lHandEnt {entt::null}
        , rHandEnt {entt::null} {
    }

    void EventHandler::onPhysicsSoundContact(entt::entity thisEnt, const worlds::PhysicsContactInfo& info) {
        auto& physSound = reg->get<PhysicsSoundComponent>(thisEnt);

        auto time = engine->getGameTime();

        if (time - physSound.lastPlayTime > 0.1) {
            worlds::AudioSystem::getInstance()->playOneShotClip(
                    physSound.soundId, info.averageContactPoint, true, glm::min(info.relativeSpeed * 0.125f, 0.9f));
            physSound.lastPlayTime = time;
        }
    }

    void EventHandler::onPhysicsSoundConstruct(entt::registry& reg, entt::entity ent) {
        auto& physEvents = reg.get_or_emplace<worlds::PhysicsEvents>(ent);
        physEvents.addContactCallback(std::bind(&EventHandler::onPhysicsSoundContact,
            this, std::placeholders::_1, std::placeholders::_2));
    }

    double clamp(double val, double min, double max) {
        return val > max ? max : (val > min ? val : min);
    }

    void EventHandler::onContactDamageDealerContact(entt::entity thisEnt, const worlds::PhysicsContactInfo& info) {
        auto& dealer = reg->get<ContactDamageDealer>(thisEnt);

        RPGStats* stats = reg->try_get<RPGStats>(info.otherEntity);

        if (stats) {
            double damagePercent = clamp((info.relativeSpeed - dealer.minVelocity) / (dealer.maxVelocity - dealer.minVelocity), 0.0, 1.0);
            logMsg("damagePercent: %.3f, relSpeed: %.3f", damagePercent, info.relativeSpeed);
            uint64_t actualDamage = dealer.damage * damagePercent;
            if (actualDamage > stats->currentHP)
                stats->currentHP = 0;
            else
                stats->currentHP -= actualDamage;

            if (stats->currentHP == 0) {
                engine->destroyNextFrame(info.otherEntity);
            }
        }
    }

    void EventHandler::onContactDamageDealerConstruct(entt::registry& reg, entt::entity ent) {
        auto& physEvents = reg.get_or_emplace<worlds::PhysicsEvents>(ent);
        physEvents.addContactCallback(std::bind(&EventHandler::onContactDamageDealerContact,
            this, std::placeholders::_1, std::placeholders::_2));
    }

    void EventHandler::init(entt::registry& registry, worlds::EngineInterfaces interfaces) {
        vrInterface = interfaces.vrInterface;
        renderer = interfaces.renderer;
        camera = interfaces.mainCamera;
        inputManager = interfaces.inputManager;
        engine = interfaces.engine;
        reg = &registry;
        playerGrabManager = new PlayerGrabManager{interfaces, registry};

        registry.on_construct<PhysicsSoundComponent>().connect<&EventHandler::onPhysicsSoundConstruct>(this);
        registry.on_construct<ContactDamageDealer>().connect<&EventHandler::onContactDamageDealerConstruct>(this);

        worlds::g_console->registerCommand(cmdToggleVsync, "r_toggleVsync", "Toggles Vsync.", renderer);
        interfaces.engine->addSystem(new ObjectParentSystem);

        lsphereSys = new LocospherePlayerSystem { interfaces, registry };
        interfaces.engine->addSystem(lsphereSys);
        interfaces.engine->addSystem(new PhysHandSystem{ interfaces, registry });

        if (enet_initialize() != 0) {
            logErr("Failed to initialize enet.");
        }

        mpManager = new MultiplayerManager{registry, isDedicated};

        new DebugArrows(registry);

        worlds::g_console->registerCommand([&](void*, const char*) {
            auto& wActor = registry.get<worlds::DynamicPhysicsActor>(lHandEnt);
            auto* body = static_cast<physx::PxRigidBody*>(wActor.actor);
            body->setLinearVelocity(physx::PxVec3{ 0.0f });

            auto& lTf = registry.get<Transform>(lHandEnt);
            auto& lPh = registry.get<PhysHand>(lHandEnt);
            auto lPose = body->getGlobalPose();
            lPose.p = worlds::glm2px(lPh.targetWorldPos);
            lTf.position = lPh.targetWorldPos;
            body->setGlobalPose(lPose);

            auto& wActorR = registry.get<worlds::DynamicPhysicsActor>(rHandEnt);
            auto* rBody = static_cast<physx::PxRigidBody*>(wActorR.actor);
            rBody->setLinearVelocity(physx::PxVec3{ 0.0f });

            auto& rTf = registry.get<Transform>(rHandEnt);
            auto& rPh = registry.get<PhysHand>(rHandEnt);
            auto rPose = rBody->getGlobalPose();
            rPose.p = worlds::glm2px(rPh.targetWorldPos);
            rTf.position = rPh.targetWorldPos;
            rBody->setGlobalPose(rPose);

            lPh.posController.reset();
            lPh.rotController.reset();

            rPh.posController.reset();
            rPh.rotController.reset();
        }, "lg_resetHands", "Resets hand PID controllers.", nullptr);
    }

    void EventHandler::preSimUpdate(entt::registry&, float) {
        g_dbgArrows->newFrame();
    }

    entt::entity fakeLHand = entt::null;
    entt::entity fakeRHand = entt::null;

    void EventHandler::update(entt::registry& reg, float deltaTime, float) {
        if (vrInterface) {
            static float yRot = 0.0f;
            static float targetYRot = 0.0f;
            static bool rotated = false;
            auto rStickInput = vrInterface->getActionV2(rStick);
            auto rotateInput = rStickInput.x;
            ImGui::Text("s: %.3f, %.3f", rStickInput.x, rStickInput.y);

            float threshold = 0.5f;
            bool rotatingNow = glm::abs(rotateInput) > threshold;

            if (rotatingNow && !rotated) {
                targetYRot += glm::radians(45.0f) * -glm::sign(rotateInput);
            }

            const float rotateSpeed = 15.0f;

            yRot += glm::clamp((targetYRot - yRot), -deltaTime * rotateSpeed, deltaTime * rotateSpeed);
            camera->rotation = glm::quat{glm::vec3{0.0f, yRot, 0.0f}};

            rotated = rotatingNow;

            if (reg.valid(audioListenerEntity)) {
                auto hmdTransform = vrInterface->getHeadTransform();
                glm::vec3 hmdPos = camera->rotation * worlds::getMatrixTranslation(hmdTransform);
                hmdPos.x = -hmdPos.x;
                hmdPos.z = -hmdPos.z;
                auto& listenerOverrideTransform = reg.get<Transform>(audioListenerEntity);
                listenerOverrideTransform.position = camera->position + hmdPos;
                listenerOverrideTransform.rotation = camera->rotation * worlds::getMatrixRotation(hmdTransform);
            }
        }

        entt::entity localLocosphereEnt = entt::null;

        reg.view<LocospherePlayerComponent>().each([&](auto ent, auto& lpc) {
            if (lpc.isLocal) {
                if (!reg.valid(localLocosphereEnt)) {
                    localLocosphereEnt = ent;
                } else {
                    logWarn("more than one local locosphere!");
                }
            }
        });

        if (!reg.valid(localLocosphereEnt)) return;

        auto& rpgStat = reg.get<RPGStats>(localLocosphereEnt);

        if (reg.valid(lHandEnt) && reg.valid(rHandEnt)) {
            auto& phl = reg.get<PhysHand>(lHandEnt);
            auto& phr = reg.get<PhysHand>(rHandEnt);

            float forceLimit = 150.0f + (100.0f * rpgStat.strength);
            float torqueLimit = 7.0f + (10.0f * rpgStat.strength);

            if (!reg.valid(phl.goingTo)) {
                phl.forceLimit = forceLimit;
                phr.forceLimit = forceLimit;
            }

            if (!reg.valid(phr.goingTo)) {
                phl.torqueLimit = torqueLimit;
                phr.torqueLimit = torqueLimit;
            }

            if (reg.valid(fakeLHand) && reg.valid(fakeRHand)) {
                auto& tfl = reg.get<Transform>(fakeLHand);
                auto& trl = reg.get<Transform>(fakeRHand);

                tfl.position = phl.targetWorldPos;
                tfl.rotation = phl.targetWorldRot;

                trl.position = phr.targetWorldPos;
                trl.rotation = phr.targetWorldRot;
            }
        }
        auto statView = reg.view<RPGStats, Transform>(entt::exclude_t<LocospherePlayerComponent>());
        ImGuiViewport* mainViewport = ImGui::GetMainViewport();
        auto size = mainViewport->Size;

        auto vpMat = camera->getProjectionMatrix(size.x / size.y) * camera->getViewMatrix();

        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        statView.each([&](entt::entity ent, RPGStats& stats, Transform& transform) {
            auto pos = transform.position + (transform.rotation * glm::vec3(0.0f, 1.8f, 0.0f));
            glm::vec4 projectedPos = vpMat * glm::vec4(pos, 1.0f);
            projectedPos /= projectedPos.w;
            bool hide = projectedPos.z < 0.0f;
            projectedPos *= 0.5f;
            projectedPos += 0.5f;
            projectedPos *= glm::vec4(size.x, size.y, 0.0f, 0.0f);
            projectedPos.y = size.y - projectedPos.y;

            ImVec2 screenPos = ImVec2(projectedPos.x, projectedPos.y);
            ImVec2 corner = ImVec2(screenPos.x + 15.0f, screenPos.y + 15.0f);

            if (!hide) {
                drawList->AddQuadFilled(
                    screenPos,
                    ImVec2(corner.x, screenPos.y),
                    corner,
                    ImVec2(screenPos.x, corner.y),
                    ImColor(0.8f, 0.0f, 0.5f)
                );

                ImVec2 textPos = ImVec2(screenPos.x, screenPos.y + 20.0f);
                std::string healthText = "Health: " + std::to_string(stats.currentHP);
                drawList->AddText(textPos, ImColor(1.0f, 1.0f, 1.0f), healthText.c_str());
            }
        });
    }

    int syncTimer = 0;

    extern void resetHand(PhysHand& ph, physx::PxRigidBody* rb);
    void EventHandler::simulate(entt::registry& registry, float simStep) {
        mpManager->simulate(simStep);

        entt::entity localLocosphereEnt = entt::null;
        LocospherePlayerComponent* localLpc = nullptr;

        registry.view<LocospherePlayerComponent>().each([&](auto ent, auto& lpc) {
            if (lpc.isLocal) {
                if (!registry.valid(localLocosphereEnt)) {
                    localLocosphereEnt = ent;
                    localLpc = &lpc;
                } else {
                    logWarn("more than one local locosphere!");
                }
            }
        });

        if (!registry.valid(localLocosphereEnt)) {
            // probably dedicated server ¯\_(ツ)_/¯
            return;
        }

        playerGrabManager->simulate(simStep);

        if (vrInterface) {
            float fenderHeight = 0.55f;
            glm::vec3 headPos = worlds::getMatrixTranslation(vrInterface->getHeadTransform());

            rHandJoint->setLocalPose(physx::PxJointActorIndex::eACTOR0, physx::PxTransform {
                    physx::PxVec3(0.0f, headPos.y - fenderHeight, 0.0f), physx::PxQuat { physx::PxIdentity }});
            lHandJoint->setLocalPose(physx::PxJointActorIndex::eACTOR0, physx::PxTransform {
                    physx::PxVec3(0.0f, headPos.y - fenderHeight, 0.0f), physx::PxQuat { physx::PxIdentity }});
            ImGui::Text("Headpos: %.3f, %.3f, %.3f", headPos.x, headPos.y, headPos.z);
        }
    }

    worlds::ConVar showTargetHands { "lg_showTargetHands", "0", "Shows devtextured hands that represent the current target transform of the hands." };

    void EventHandler::onSceneStart(entt::registry& registry) {
        registry.view<worlds::DynamicPhysicsActor>().each([&](auto ent, auto&) {
            registry.emplace<SyncedRB>(ent);
        });

        // create our lil' pal the player
        if (!isDedicated && registry.view<PlayerStartPoint>().size() > 0) {
            entt::entity pspEnt = registry.view<PlayerStartPoint, Transform>().front();
            Transform& pspTf = registry.get<Transform>(pspEnt);

            PlayerRig rig = lsphereSys->createPlayerRig(registry, pspTf.position);
            auto& lpc = registry.get<LocospherePlayerComponent>(rig.locosphere);
            lpc.isLocal = true;
            lpc.sprint = false;
            lpc.maxSpeed = 0.0f;
            lpc.xzMoveInput = glm::vec2(0.0f, 0.0f);
            auto& stats = registry.emplace<RPGStats>(rig.locosphere);
            stats.strength = 15;

            playerGrabManager->setPlayerEntity(rig.locosphere);

            logMsg("Created player rig");

            if (vrInterface) {
                rStick = vrInterface->getActionHandle("/actions/main/in/RStick");
                camera->rotation = glm::quat{};
            }

            auto& fenderTransform = registry.get<Transform>(rig.fender);
            auto matId = worlds::g_assetDB.addOrGetExisting("Materials/VRHands/placeholder.json");
            auto devMatId = worlds::g_assetDB.addOrGetExisting("Materials/dev.json");
            auto lHandModel = worlds::g_assetDB.addOrGetExisting("Models/VRHands/hand_placeholder_l.wmdl");
            auto rHandModel = worlds::g_assetDB.addOrGetExisting("Models/VRHands/hand_placeholder_r.wmdl");

            lHandEnt = registry.create();
            registry.get<PlayerRig>(rig.locosphere).lHand = lHandEnt;
            registry.emplace<worlds::WorldObject>(lHandEnt, matId, lHandModel);
            auto& lht = registry.emplace<Transform>(lHandEnt);
            lht.position = glm::vec3(0.5, 0.0f, 0.0f) + fenderTransform.position;
            registry.emplace<worlds::NameComponent>(lHandEnt).name = "L. Handy";

            if (showTargetHands.getInt()) {
                fakeLHand = registry.create();
                registry.emplace<worlds::WorldObject>(fakeLHand, devMatId, lHandModel);
                registry.emplace<Transform>(fakeLHand);
                registry.emplace<worlds::NameComponent>(fakeLHand).name = "Fake L. Handy";
            }

            rHandEnt = registry.create();
            registry.get<PlayerRig>(rig.locosphere).rHand = rHandEnt;
            registry.emplace<worlds::WorldObject>(rHandEnt, matId, rHandModel);
            auto& rht = registry.emplace<Transform>(rHandEnt);
            rht.position = glm::vec3(-0.5f, 0.0f, 0.0f) + fenderTransform.position;
            registry.emplace<worlds::NameComponent>(rHandEnt).name = "R. Handy";

            if (showTargetHands.getInt()) {
                fakeRHand = registry.create();
                registry.emplace<worlds::WorldObject>(fakeRHand, devMatId, rHandModel);
                registry.emplace<Transform>(fakeRHand);
                registry.emplace<worlds::NameComponent>(fakeRHand).name = "Fake R. Handy";
            }

            auto lActor = worlds::g_physics->createRigidDynamic(physx::PxTransform{ physx::PxIdentity });
            // Using the reference returned by this doesn't work unfortunately.
            registry.emplace<worlds::DynamicPhysicsActor>(lHandEnt, lActor);

            auto rActor = worlds::g_physics->createRigidDynamic(physx::PxTransform{ physx::PxIdentity });
            auto& rwActor = registry.emplace<worlds::DynamicPhysicsActor>(rHandEnt, rActor);
            auto& lwActor = registry.get<worlds::DynamicPhysicsActor>(lHandEnt);

            rwActor.physicsShapes.emplace_back(worlds::PhysicsShape::boxShape(glm::vec3{ 0.025f, 0.045f, 0.05f }));
            rwActor.physicsShapes[0].pos = glm::vec3{0.0f, 0.0f, 0.03f};
            lwActor.physicsShapes.emplace_back(worlds::PhysicsShape::boxShape(glm::vec3{ 0.025f, 0.045f, 0.05f }));
            lwActor.physicsShapes[0].pos = glm::vec3{0.0f, 0.0f, 0.03f};

            rwActor.layer = worlds::PLAYER_PHYSICS_LAYER;
            lwActor.layer = worlds::PLAYER_PHYSICS_LAYER;

            worlds::updatePhysicsShapes(rwActor);
            worlds::updatePhysicsShapes(lwActor);

            rActor->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, true);
            lActor->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, true);

            worlds::g_scene->addActor(*rActor);
            worlds::g_scene->addActor(*lActor);

            physx::PxRigidBodyExt::setMassAndUpdateInertia(*rActor, 2.0f);
            physx::PxRigidBodyExt::setMassAndUpdateInertia(*lActor, 2.0f);

            PIDSettings posSettings{ 750.0f, 600.0f, 137.0f };
            PIDSettings rotSettings{ 200.0f, 0.0f, 29.0f };

            auto& lHandPhys = registry.emplace<PhysHand>(lHandEnt);
            lHandPhys.locosphere = rig.locosphere;
            lHandPhys.posController.acceptSettings(posSettings);
            lHandPhys.posController.averageAmount = 5.0f;
            lHandPhys.rotController.acceptSettings(rotSettings);
            lHandPhys.rotController.averageAmount = 2.0f;
            lHandPhys.follow = FollowHand::LeftHand;

            auto& rHandPhys = registry.emplace<PhysHand>(rHandEnt);

            rHandPhys.locosphere = rig.locosphere;
            rHandPhys.posController.acceptSettings(posSettings);
            rHandPhys.posController.averageAmount = 5.0f;
            rHandPhys.rotController.acceptSettings(rotSettings);
            rHandPhys.rotController.averageAmount = 2.0f;
            rHandPhys.follow = FollowHand::RightHand;

            auto fenderActor = registry.get<worlds::DynamicPhysicsActor>(rig.fender).actor;

            lHandJoint = physx::PxD6JointCreate(*worlds::g_physics, fenderActor, physx::PxTransform { physx::PxIdentity }, lActor,
            physx::PxTransform { physx::PxIdentity });

            lHandJoint->setLinearLimit(physx::PxJointLinearLimit{
                    physx::PxTolerancesScale{}, 0.8f});
            lHandJoint->setMotion(physx::PxD6Axis::eX, physx::PxD6Motion::eLIMITED);
            lHandJoint->setMotion(physx::PxD6Axis::eY, physx::PxD6Motion::eLIMITED);
            lHandJoint->setMotion(physx::PxD6Axis::eZ, physx::PxD6Motion::eLIMITED);
            lHandJoint->setMotion(physx::PxD6Axis::eSWING1, physx::PxD6Motion::eFREE);
            lHandJoint->setMotion(physx::PxD6Axis::eSWING2, physx::PxD6Motion::eFREE);
            lHandJoint->setMotion(physx::PxD6Axis::eTWIST, physx::PxD6Motion::eFREE);

            rHandJoint = physx::PxD6JointCreate(*worlds::g_physics, fenderActor, physx::PxTransform { physx::PxIdentity }, rActor,
            physx::PxTransform { physx::PxIdentity });

            rHandJoint->setLinearLimit(physx::PxJointLinearLimit{
                    physx::PxTolerancesScale{}, 0.8f});
            rHandJoint->setMotion(physx::PxD6Axis::eX, physx::PxD6Motion::eLIMITED);
            rHandJoint->setMotion(physx::PxD6Axis::eY, physx::PxD6Motion::eLIMITED);
            rHandJoint->setMotion(physx::PxD6Axis::eZ, physx::PxD6Motion::eLIMITED);
            rHandJoint->setMotion(physx::PxD6Axis::eSWING1, physx::PxD6Motion::eFREE);
            rHandJoint->setMotion(physx::PxD6Axis::eSWING2, physx::PxD6Motion::eFREE);
            rHandJoint->setMotion(physx::PxD6Axis::eTWIST, physx::PxD6Motion::eFREE);
            lActor->setSolverIterationCounts(16, 8);
            rActor->setSolverIterationCounts(16, 8);
            lActor->setLinearVelocity(physx::PxVec3{0.0f});
            rActor->setLinearVelocity(physx::PxVec3{0.0f});

            rHandJoint->setLocalPose(physx::PxJointActorIndex::eACTOR0, physx::PxTransform {
                physx::PxVec3 { 0.0f, 0.8f, 0.0f },
                physx::PxQuat { physx::PxIdentity }
            });

            lHandJoint->setLocalPose(physx::PxJointActorIndex::eACTOR0, physx::PxTransform {
                physx::PxVec3 { 0.0f, 0.8f, 0.0f },
                physx::PxQuat { physx::PxIdentity }
            });

            PhysicsSoundComponent& lPsc = reg->emplace<PhysicsSoundComponent>(lHandEnt);
            lPsc.soundId = worlds::g_assetDB.addOrGetExisting("Audio/SFX/Player/hand_slap1.ogg");
            PhysicsSoundComponent& rPsc = reg->emplace<PhysicsSoundComponent>(rHandEnt);
            rPsc.soundId = worlds::g_assetDB.addOrGetExisting("Audio/SFX/Player/hand_slap1.ogg");

            ContactDamageDealer& lCdd = reg->emplace<ContactDamageDealer>(lHandEnt);
            lCdd.damage = 7;
            lCdd.minVelocity = 7.5f;
            lCdd.maxVelocity = 12.0f;

            ContactDamageDealer& rCdd = reg->emplace<ContactDamageDealer>(rHandEnt);
            rCdd.damage = 7;
            rCdd.minVelocity = 7.5f;
            rCdd.maxVelocity = 12.0f;

            if (vrInterface) {
                audioListenerEntity = registry.create();
                registry.emplace<Transform>(audioListenerEntity);
                registry.emplace<worlds::AudioListenerOverride>(audioListenerEntity);
            }
        }

        if (isDedicated) {
            mpManager->onSceneStart(registry);
        }

        g_dbgArrows->createEntities();
    }

    void EventHandler::shutdown(entt::registry& registry) {
        if (registry.valid(lHandEnt)) {
            registry.destroy(lHandEnt);
            registry.destroy(rHandEnt);
        }

        if (client)
            delete client;

        if (server)
            delete server;

        if (playerGrabManager)
            delete playerGrabManager;

        enet_deinitialize();
    }
}
