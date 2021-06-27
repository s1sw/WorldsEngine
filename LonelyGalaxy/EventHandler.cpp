#include "EventHandler.hpp"
#include <Util/RichPresence.hpp>
#include <Core/Log.hpp>
#include <Render/Render.hpp>
#include "Core/AssetDB.hpp"
#include "DebugArrow.hpp"
#include "Physics/D6Joint.hpp"
#include "Physics/PhysicsActor.hpp"
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
#include "PlayerSoundSystem.hpp"
#include "ObjectParentSystem.hpp"
#include "extensions/PxD6Joint.h"
#include "Util/VKImGUIUtil.hpp"
#include "MathsUtil.hpp"
#include "PlayerStartPoint.hpp"
#include "RPGStats.hpp"
#include "GripPoint.hpp"
#include <Input/Input.hpp>
#include "PhysicsSoundComponent.hpp"
#include <Audio/Audio.hpp>
#include "PlayerGrabManager.hpp"
#include "ContactDamageDealer.hpp"
#include <UI/WorldTextComponent.hpp>
#include "Grabbable.hpp"
#include <Serialization/SceneSerialization.hpp>
#include "DamagingProjectile.hpp"
#include "Gun.hpp"
#include <Libs/pcg_basic.h>
#include "StabbySystem.hpp"
#include "StatDisplayInfo.hpp"
#include "Enemies/DroneAI.hpp"

namespace lg {
    worlds::RTTPass* spectatorPass = nullptr;
    worlds::Camera spectatorCam;
    worlds::ConVar enableSpectatorCam { "lg_enableVrSpectatorCam", "0", "Enables VR spectator camera." };
    worlds::ConVar useCamcorder { "lg_useCamcorder", "0", "Uses the camcorder for the screen view." };
    struct SyncedRB {};
    struct StatDisplay {
        entt::entity textEntity;
    };

    struct DamageNumber {
        glm::vec3 velocity;
        double spawnTime;
    };

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

    void addStatDisplayIfNeeded(entt::registry& reg, entt::entity entity) {
        if (!reg.has<StatDisplay>(entity) && !reg.has<LocospherePlayerComponent>(entity)) {
            StatDisplay& sd = reg.emplace<StatDisplay>(entity);
            sd.textEntity = reg.create();

            reg.emplace<Transform>(sd.textEntity);
            worlds::WorldTextComponent& wtc = reg.emplace<worlds::WorldTextComponent>(sd.textEntity);
            wtc.textScale = 0.005f;
        }
    }

    float randFloatZeroOne() {
        return ((float)(pcg32_random() / (double)UINT32_MAX)) * 2.0f - 1.0f;
    }

    void spawnDamageNumber(double currentTime, entt::registry& reg, uint64_t damage, glm::vec3 location) {
        glm::vec3 jumpDir { randFloatZeroOne(), randFloatZeroOne(), randFloatZeroOne() };

        entt::entity ent = reg.create();
        Transform& transform = reg.emplace<Transform>(ent);
        transform.position = location;

        DamageNumber& dn = reg.emplace<DamageNumber>(ent);
        dn.velocity = glm::normalize(jumpDir);
        dn.spawnTime = currentTime;

        worlds::WorldTextComponent& wtc = reg.emplace<worlds::WorldTextComponent>(ent);
        wtc.textScale = 0.004f;
        wtc.text = std::to_string(damage);
    }

    void EventHandler::onContactDamageDealerContact(entt::entity thisEnt, const worlds::PhysicsContactInfo& info) {
        auto& dealer = reg->get<ContactDamageDealer>(thisEnt);

        RPGStats* stats = reg->try_get<RPGStats>(info.otherEntity);

        if (stats && info.relativeSpeed > dealer.minVelocity) {
            addStatDisplayIfNeeded(*reg, info.otherEntity);

            double damagePercent = clamp((info.relativeSpeed - dealer.minVelocity) / (dealer.maxVelocity - dealer.minVelocity), 0.0, 1.0);
            logMsg("damagePercent: %.3f, relSpeed: %.3f", damagePercent, info.relativeSpeed);
            uint64_t actualDamage = dealer.damage * damagePercent;
            stats->damage(actualDamage);
            spawnDamageNumber(engine->getGameTime(), *reg, actualDamage, info.averageContactPoint - (info.normal * 0.1f));

            if (stats->currentHP == 0) {
                engine->destroyNextFrame(info.otherEntity);

                if (reg->has<StatDisplay>(info.otherEntity)) {
                    engine->destroyNextFrame(reg->get<StatDisplay>(info.otherEntity).textEntity);
                }
            }
        }
    }

    void EventHandler::onContactDamageDealerConstruct(entt::registry& reg, entt::entity ent) {
        auto& physEvents = reg.get_or_emplace<worlds::PhysicsEvents>(ent);
        physEvents.addContactCallback(std::bind(&EventHandler::onContactDamageDealerContact,
            this, std::placeholders::_1, std::placeholders::_2));
    }

    void fireGun(entt::registry& reg, entt::entity ent) {
        Gun& gun = reg.get<Gun>(ent);
        Transform& gunTransform = reg.get<Transform>(ent);
        worlds::DynamicPhysicsActor& gunDpa = reg.get<worlds::DynamicPhysicsActor>(ent);

        worlds::AudioSystem::getInstance()->playOneShotClip(
            worlds::AssetDB::pathToId("Audio/SFX/gunshot.ogg"), gunTransform.position, true
        );

        worlds::AssetID projectileId = worlds::AssetDB::pathToId("Prefabs/gun_projectile.wprefab");
        entt::entity projectile = worlds::SceneLoader::createPrefab(projectileId, reg);

        Transform& projectileTransform = reg.get<Transform>(projectile);
        Transform firePointTransform = gun.firePoint.transformBy(gunTransform);

        glm::vec3 forward = firePointTransform.rotation * glm::vec3(0.0f, 0.0f, 1.0f);


        projectileTransform.position = firePointTransform.position;
        projectileTransform.rotation = firePointTransform.rotation;

        worlds::DynamicPhysicsActor& dpa = reg.get<worlds::DynamicPhysicsActor>(projectile);

        gunDpa.actor->addForce(worlds::glm2px(-forward * 100.0f * dpa.mass), physx::PxForceMode::eIMPULSE);

        dpa.actor->setGlobalPose(worlds::glm2px(projectileTransform));
        dpa.actor->addForce(worlds::glm2px(forward * 100.0f), physx::PxForceMode::eVELOCITY_CHANGE);
    }

    void EventHandler::onGunConstruct(entt::registry& r, entt::entity ent) {
        Grabbable& grabbable = r.get<Grabbable>(ent);

        grabbable.onTriggerPressed = [&](entt::entity ent) {
            Gun& gun = reg->get<Gun>(ent);
            if (gun.automatic) return;

            double time = engine->getGameTime();

            if (time - gun.lastFireTime < gun.shotPeriod) return;
            fireGun(*reg, ent);
            gun.lastFireTime = time;
        };

        grabbable.onTriggerHeld = [&](entt::entity ent) {
            Gun& gun = reg->get<Gun>(ent);
            if (!gun.automatic) return;

            double time = engine->getGameTime();

            if (time - gun.lastFireTime < gun.shotPeriod) return;
            fireGun(*reg, ent);
            gun.lastFireTime = time;
        };
    }

    void EventHandler::onProjectileConstruct(entt::registry& reg, entt::entity ent) {
        auto& physEvents = reg.get_or_emplace<worlds::PhysicsEvents>(ent);
        DamagingProjectile& projectile = reg.get<DamagingProjectile>(ent);
        projectile.creationTime = engine->getGameTime();

        physEvents.addContactCallback([&](entt::entity thisEnt, const worlds::PhysicsContactInfo& info) {
            DamagingProjectile& projectile = reg.get<DamagingProjectile>(thisEnt);
            engine->destroyNextFrame(thisEnt);

            RPGStats* stats = reg.try_get<RPGStats>(info.otherEntity);

            if (stats) {
                addStatDisplayIfNeeded(reg, info.otherEntity);
                stats->damage(projectile.damage);

                spawnDamageNumber(engine->getGameTime(), reg, 15, info.averageContactPoint + (info.normal * 0.25f));

                if (stats->currentHP == 0) {
                    engine->destroyNextFrame(info.otherEntity);

                    if (reg.has<StatDisplay>(info.otherEntity)) {
                        engine->destroyNextFrame(reg.get<StatDisplay>(info.otherEntity).textEntity);
                    }
                }
            }
        });
    }

    entt::entity camcorder = entt::null;

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
        registry.on_construct<Gun>().connect<&EventHandler::onGunConstruct>(this);
        registry.on_construct<DamagingProjectile>().connect<&EventHandler::onProjectileConstruct>(this);

        worlds::g_console->registerCommand(cmdToggleVsync, "r_toggleVsync", "Toggles Vsync.", renderer);
        interfaces.engine->addSystem(new ObjectParentSystem);

        lsphereSys = new LocospherePlayerSystem { interfaces, registry };
        interfaces.engine->addSystem(lsphereSys);
        interfaces.engine->addSystem(new PhysHandSystem{ interfaces, registry });
        interfaces.engine->addSystem(new StabbySystem { interfaces, registry });
        interfaces.engine->addSystem(new PlayerSoundSystem { interfaces, registry });
        interfaces.engine->addSystem(new DroneAISystem { interfaces, registry });

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

        worlds::g_console->registerCommand([&](void*, const char*) {
            camcorder = worlds::SceneLoader::createPrefab(worlds::AssetDB::pathToId("Prefabs/spectator_camcorder.wprefab"), registry);
        }, "lg_spawnCamcorder", "Spawns the camcorder.", nullptr);
    }

    void EventHandler::preSimUpdate(entt::registry&, float) {
        g_dbgArrows->newFrame();
    }

    entt::entity fakeLHand = entt::null;
    entt::entity fakeRHand = entt::null;
    entt::entity headPlaceholder = entt::null;

    Transform getHmdTransform(worlds::Camera* camera, worlds::IVRInterface* vrInterface) {
        const glm::vec3 flipVec { -1.0f, 1.0f, -1.0f };

        auto hmdTransform = vrInterface->getHeadTransform();
        glm::vec3 hmdPos = camera->rotation * worlds::getMatrixTranslation(hmdTransform);
        hmdPos *= flipVec;

        glm::quat hmdRot = worlds::getMatrixRotation(hmdTransform);
        glm::vec3 hmdRotationAxis = glm::axis(hmdRot) * flipVec;
        float hmdRotationAngle = glm::angle(hmdRot);

        return Transform {
            camera->position + hmdPos,
            camera->rotation * glm::angleAxis(hmdRotationAngle, hmdRotationAxis)
        };
    }

    Transform getHandTargetTransform(worlds::Camera* cam, worlds::IVRInterface* vrInterface, worlds::Hand wHand) {
        static glm::vec3 posOffset { 0.0f, 0.0f, -0.05f };
        static glm::vec3 rotEulerOffset { -120.0f, 0.0f, -51.0f };
        Transform t;
        if (vrInterface->getHandTransform(wHand, t)) {
            t.position += t.rotation * posOffset;
            glm::quat flip180 = glm::angleAxis(glm::pi<float>(), glm::vec3{0.0f, 1.0f, 0.0f});
            glm::quat correctedRot = cam->rotation * flip180;
            t.position = correctedRot * t.position;
            t.position += cam->position;
            t.rotation *= glm::quat{glm::radians(rotEulerOffset)};
            t.rotation = flip180 * cam->rotation * t.rotation;
        }

        return t;
    }

    void EventHandler::update(entt::registry& reg, float deltaTime, float) {
        if (vrInterface) {
            if (enableSpectatorCam.getInt()) {
                if (!spectatorPass || !spectatorPass->isValid) {
                    if (spectatorPass)
                        renderer->destroyRTTPass(spectatorPass);
                    int w, h;
                    SDL_GetWindowSize(engine->getMainWindow(), &w, &h);
                    worlds::RTTPassCreateInfo ci {
                        .cam = &spectatorCam,
                        .width = static_cast<uint32_t>(w),
                        .height = static_cast<uint32_t>(h),
                        .isVr = false,
                        .useForPicking = false,
                        .enableShadows = true,
                        .outputToScreen = true
                    };
                    spectatorPass = renderer->createRTTPass(ci);
                    spectatorPass->drawSortKey = 1;
                }
            }

            if (spectatorPass) {
                spectatorPass->active = enableSpectatorCam.getInt();
                if (!enableSpectatorCam.getInt()) {
                    renderer->destroyRTTPass(spectatorPass);
                    spectatorPass = nullptr;
                }
            }
            static float yRot = 0.0f;
            static float targetYRot = 0.0f;
            static bool rotated = false;
            auto rStickInput = vrInterface->getActionV2(rStick);
            auto rotateInput = rStickInput.x;

            float threshold = 0.5f;
            bool rotatingNow = glm::abs(rotateInput) > threshold;

            if (rotatingNow && !rotated) {
                targetYRot += glm::radians(45.0f) * -glm::sign(rotateInput);
            }

            const float rotateSpeed = 15.0f;

            yRot += glm::clamp((targetYRot - yRot), -deltaTime * rotateSpeed, deltaTime * rotateSpeed);
            camera->rotation = glm::quat{glm::vec3{0.0f, yRot, 0.0f}};

            rotated = rotatingNow;

            Transform hmdTransform = getHmdTransform(camera, vrInterface);

            if (reg.valid(audioListenerEntity)) {
                auto& listenerOverrideTransform = reg.get<Transform>(audioListenerEntity);
                listenerOverrideTransform = hmdTransform;
            }

            if (reg.valid(headPlaceholder)) {
                auto& t = reg.get<Transform>(headPlaceholder);
                t = hmdTransform;
            }

            if (!useCamcorder.getInt() || !reg.valid(camcorder)) {
                spectatorCam.position = hmdTransform.position;
                spectatorCam.rotation = hmdTransform.rotation;
            } else {
                Transform& camcorderTransform = reg.get<Transform>(camcorder);
                spectatorCam.position = camcorderTransform.position;
                spectatorCam.rotation = camcorderTransform.rotation;
            }
        } else {
            if (reg.valid(headPlaceholder)) {
                auto& t = reg.get<Transform>(headPlaceholder);
                t.position = camera->position;
                t.rotation = camera->rotation;
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

        reg.view<DamagingProjectile>().each([&](entt::entity ent, DamagingProjectile& dp) {
            if (engine->getGameTime() - dp.creationTime > 10.0)
                engine->destroyNextFrame(ent);
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
                tfl = getHandTargetTransform(camera, vrInterface, worlds::Hand::LeftHand);
                trl = getHandTargetTransform(camera, vrInterface, worlds::Hand::RightHand);
            }
        }
        auto statDisplayView = reg.view<StatDisplay, StatDisplayInfo, RPGStats, Transform>();
        glm::vec3 headPos = camera->position;

        if (vrInterface) {
            Transform hmdTransform = getHmdTransform(camera, vrInterface);
            headPos = hmdTransform.position;
        }

        reg.view<DamageNumber, Transform>().each([&](entt::entity ent, DamageNumber& dn, Transform& transform) {
            dn.velocity += glm::vec3(0.0f, -9.81f, 0.0f) * deltaTime;
            transform.position += dn.velocity * deltaTime;
            transform.rotation = safeQuatLookat(glm::normalize(transform.position - headPos));

            if (engine->getGameTime() - dn.spawnTime > 5.0) {
                engine->destroyNextFrame(ent);
            }
        });

        statDisplayView.each([&](StatDisplay& sd, StatDisplayInfo& sdi, RPGStats& stats, Transform& transform) {
            glm::vec3 displayPos = transform.position + (transform.rotation * sdi.offset);
            entt::entity textEnt = sd.textEntity;

            Transform& statTextTransform = reg.get<Transform>(textEnt);
            statTextTransform.position = displayPos;
            statTextTransform.rotation = safeQuatLookat(glm::normalize(displayPos - headPos));

            worlds::WorldTextComponent& wtc = reg.get<worlds::WorldTextComponent>(textEnt);
            std::string healthText = "Health: " + std::to_string(stats.currentHP);
            wtc.text = healthText;
            wtc.dirty = true;
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
            // probably dedicated server
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
            //ImGui::Text("Headpos: %.3f, %.3f, %.3f", headPos.x, headPos.y, headPos.z);
        }
    }

    worlds::ConVar showTargetHands { "lg_showTargetHands", "0", "Shows devtextured hands that represent the current target transform of the hands." };

    void EventHandler::onSceneStart(entt::registry& registry) {
        camcorder = entt::null;
        registry.view<worlds::DynamicPhysicsActor>().each([&](auto ent, auto&) {
            registry.emplace_or_replace<SyncedRB>(ent);
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
            }

            auto& fenderTransform = registry.get<Transform>(rig.fender);
            auto matId = worlds::AssetDB::pathToId("Materials/VRHands/placeholder.json");
            auto devMatId = worlds::AssetDB::pathToId("Materials/dev.json");
            auto lHandModel = worlds::AssetDB::pathToId("Models/VRHands/hand_placeholder_l.wmdl");
            auto rHandModel = worlds::AssetDB::pathToId("Models/VRHands/hand_placeholder_r.wmdl");

            //headPlaceholder = registry.create();
            //registry.emplace<Transform>(headPlaceholder);
            //registry.emplace<worlds::WorldObject>(headPlaceholder, devMatId, worlds::AssetDB::pathToId("Models/head placeholder.obj"));

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

            PIDSettings posSettings{ 2250.0f, 600.0f, 70.0f };
            PIDSettings rotSettings{ 200.0f, 300.0f, 29.0f };

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
            lPsc.soundId = worlds::AssetDB::pathToId("Audio/SFX/Player/hand_slap1.ogg");
            PhysicsSoundComponent& rPsc = reg->emplace<PhysicsSoundComponent>(rHandEnt);
            rPsc.soundId = worlds::AssetDB::pathToId("Audio/SFX/Player/hand_slap1.ogg");

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
        camera->rotation = glm::quat{glm::vec3{0.0f}};
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
