#include "ConvergeEventHandler.hpp"
#include <openvr.h>
#include <Log.hpp>
#include <Render.hpp>
#include "AssetDB.hpp"
#include "DebugArrow.hpp"
#include "SourceModelLoader.hpp"
#include "Transform.hpp"
#include <OpenVRInterface.hpp>
#include <Physics.hpp>
#include <Console.hpp>
#include <imgui.h>
#include <MatUtil.hpp>
#include <Engine.hpp>
#include "NameComponent.hpp"
#include "LocospherePlayerSystem.hpp"
#include "PhysHandSystem.hpp"
#include <enet/enet.h>
#include <JobSystem.hpp>
#include "CreateModelObject.hpp"
#include "ObjectParentSystem.hpp"

namespace converge {
    const uint16_t CONVERGE_PORT = 3011;

    void cmdToggleVsync(void* obj, const char*) {
        auto renderer = (worlds::VKRenderer*)obj;
        renderer->setVsync(!renderer->getVsync());
    }

    EventHandler::EventHandler(bool dedicatedServer) 
        : isDedicated {dedicatedServer}
        , enetHost {nullptr}
        , client {nullptr}
        , lHandEnt {entt::null}
        , rHandEnt {entt::null} {
    }

    void EventHandler::init(entt::registry& registry, worlds::EngineInterfaces interfaces) {
        vrInterface = interfaces.vrInterface;
        renderer = interfaces.renderer;
        camera = interfaces.mainCamera;
        inputManager = interfaces.inputManager;

        worlds::g_console->registerCommand(cmdToggleVsync, "r_toggleVsync", "Toggles Vsync.", renderer);
        interfaces.engine->addSystem(new ObjectParentSystem);

        lsphereSys = new LocospherePlayerSystem { interfaces, registry };
        interfaces.engine->addSystem(lsphereSys);
        interfaces.engine->addSystem(new PhysHandSystem{ interfaces, registry });

        if (enet_initialize() != 0) {
            logErr("Failed to initialize enet.");
        }

        if (isDedicated) {
            server = new Server{};
            server->start();
        }

        worlds::g_console->registerCommand([&](void*, const char* arg) {
            if (isDedicated) {
                logErr("this is a server! what are you trying to do???");
                return;
            }

            client = new Client{};
            
            // assume the argument is an address
            ENetAddress addr;
            enet_address_set_host(&addr, arg);
            addr.port = CONVERGE_PORT;

            client->connect(addr);
            }, "cnvrg_connect", "Connects to the specified server.", nullptr);

        new DebugArrows(registry);

        if (vrInterface) { 
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
            }, "cnvrg_resetHands", "Resets hand PID controllers.", nullptr);
        }
    }

    glm::vec3 offset {0.0f, 0.01f, 0.1f};
    void EventHandler::preSimUpdate(entt::registry& registry, float deltaTime) {
    }

    void EventHandler::update(entt::registry& registry, float deltaTime, float interpAlpha) {
        if (isDedicated)
            server->processMessages(nullptr);

        if (client)
            client->processMessages(nullptr);

        g_dbgArrows->newFrame();
        entt::entity localLocosphereEnt = entt::null;

        registry.view<LocospherePlayerComponent>().each([&](auto ent, auto& lpc) {
            if (lpc.isLocal) {
                if (!registry.valid(localLocosphereEnt))
                    localLocosphereEnt = ent;
                else {
                    logWarn("more than one local locosphere!");
                }
            }
        });

        if (!registry.valid(localLocosphereEnt)) {
            // probably dedicated server ¯\_(ツ)_/¯
            return;
        }

        auto& t = registry.get<Transform>(localLocosphereEnt);
        auto& t2 = registry.get<Transform>(otherLocosphere);
        auto& lpc2 = registry.get<LocospherePlayerComponent>(otherLocosphere);
        glm::vec3 dir = t.position - t2.position;
        float sqDist = glm::length2(dir);
        dir.y = 0.0f;
        dir = glm::normalize(dir);

        ImGui::Text("dir: %.3f, %.3f, %.3f", dir.x, dir.y, dir.z);

        if (sqDist > 16.0f)
            lpc2.xzMoveInput = glm::vec2 { dir.x, dir.z };
        else
            lpc2.xzMoveInput = glm::vec2 { 0.0f };
        lpc2.sprint = sqDist > 100.0f; 

    }

    void EventHandler::simulate(entt::registry&, float) {
    }

    void EventHandler::onSceneStart(entt::registry& registry) {
        // create... a SECOND LOCOSPHERE :O
        PlayerRig other = lsphereSys->createPlayerRig(registry);
        auto& lpc = registry.get<LocospherePlayerComponent>(other.locosphere);
        lpc.isLocal = false;
        lpc.sprint = false;
        lpc.maxSpeed = 0.0f;
        lpc.xzMoveInput = glm::vec2(0.0f, 0.0f);
        otherLocosphere = other.locosphere;

        auto meshId = worlds::g_assetDB.addOrGetExisting("sourcemodel/models/konnie/isa/detroit/connor.mdl");
        auto devMatId = worlds::g_assetDB.addOrGetExisting("Materials/dev.json");
        auto& connorWO = registry.emplace<worlds::WorldObject>(other.locosphere, devMatId, meshId);
        worlds::setupSourceMaterials(meshId, connorWO);

        // create our lil' pal the player
        if (!isDedicated) {
            PlayerRig other = lsphereSys->createPlayerRig(registry);
            auto& lpc = registry.get<LocospherePlayerComponent>(other.locosphere);
            lpc.isLocal = true;
            lpc.sprint = false;
            lpc.maxSpeed = 0.0f;
            lpc.xzMoveInput = glm::vec2(0.0f, 0.0f);

            if (vrInterface) {
                auto matId = worlds::g_assetDB.addOrGetExisting("Materials/dev.json");
                auto saberId = worlds::g_assetDB.addOrGetExisting("saber.wmdl");

                lHandEnt = registry.create();
                auto& lhWO = registry.emplace<worlds::WorldObject>(lHandEnt, matId, saberId);
                lhWO.materials[0] = worlds::g_assetDB.addOrGetExisting("Materials/saber_blade.json");
                lhWO.materials[1] = matId;
                lhWO.presentMaterials[1] = true;
                auto& lht = registry.emplace<Transform>(lHandEnt);
                lht.position = glm::vec3(0.5f, 1.0f, 0.0f);
                registry.emplace<worlds::NameComponent>(lHandEnt).name = "L. Handy";

                rHandEnt = registry.create();
                auto& rhWO = registry.emplace<worlds::WorldObject>(rHandEnt, matId, saberId);
                rhWO.materials[0] = worlds::g_assetDB.addOrGetExisting("Materials/saber_blade.json");
                rhWO.materials[1] = matId;
                rhWO.presentMaterials[1] = true;
                auto& rht = registry.emplace<Transform>(rHandEnt);
                rht.position = glm::vec3(-0.5f, 1.0f, 0.0f);
                registry.emplace<worlds::NameComponent>(rHandEnt).name = "R. Handy";

                auto lActor = worlds::g_physics->createRigidDynamic(physx::PxTransform{ physx::PxIdentity });
                // Using the reference returned by this doesn't work unfortunately.
                registry.emplace<worlds::DynamicPhysicsActor>(lHandEnt, lActor);

                auto rActor = worlds::g_physics->createRigidDynamic(physx::PxTransform{ physx::PxIdentity });
                auto& rwActor = registry.emplace<worlds::DynamicPhysicsActor>(rHandEnt, rActor);
                auto& lwActor = registry.get<worlds::DynamicPhysicsActor>(lHandEnt);

                rwActor.physicsShapes.emplace_back(worlds::PhysicsShape::sphereShape(0.1f));
                lwActor.physicsShapes.emplace_back(worlds::PhysicsShape::sphereShape(0.1f));

                worlds::updatePhysicsShapes(rwActor);
                worlds::updatePhysicsShapes(lwActor);

                worlds::g_scene->addActor(*rActor);
                worlds::g_scene->addActor(*lActor);

                physx::PxRigidBodyExt::setMassAndUpdateInertia(*rActor, 2.0f);
                physx::PxRigidBodyExt::setMassAndUpdateInertia(*lActor, 2.0f);

                auto& lHandPhys = registry.emplace<PhysHand>(lHandEnt);
                auto& rHandPhys = registry.emplace<PhysHand>(rHandEnt);

                lHandPhys.locosphere = other.locosphere;
                rHandPhys.locosphere = other.locosphere;

                PIDSettings posSettings {1370.0f, 0.0f, 100.0f};
                PIDSettings rotSettings {2.5f, 0.0f, 0.2f};

                lHandPhys.posController.acceptSettings(posSettings);
                lHandPhys.rotController.acceptSettings(rotSettings);

                rHandPhys.posController.acceptSettings(posSettings);
                rHandPhys.rotController.acceptSettings(rotSettings);

                lHandPhys.follow = FollowHand::LeftHand;
                rHandPhys.follow = FollowHand::RightHand;

                auto fenderActor = registry.get<worlds::DynamicPhysicsActor>(other.fender).actor;

                lHandJoint = physx::PxD6JointCreate(*worlds::g_physics, fenderActor, physx::PxTransform { physx::PxIdentity }, lActor, physx::PxTransform { physx::PxIdentity });
                lHandJoint->setLinearLimit(physx::PxJointLinearLimit{physx::PxTolerancesScale{}, 1.25f});
                lHandJoint->setMotion(physx::PxD6Axis::eX, physx::PxD6Motion::eLIMITED);
                lHandJoint->setMotion(physx::PxD6Axis::eY, physx::PxD6Motion::eLIMITED);
                lHandJoint->setMotion(physx::PxD6Axis::eZ, physx::PxD6Motion::eLIMITED);
                lHandJoint->setMotion(physx::PxD6Axis::eSWING1, physx::PxD6Motion::eFREE);
                lHandJoint->setMotion(physx::PxD6Axis::eSWING2, physx::PxD6Motion::eFREE);
                lHandJoint->setMotion(physx::PxD6Axis::eTWIST, physx::PxD6Motion::eFREE);

                rHandJoint = physx::PxD6JointCreate(*worlds::g_physics, fenderActor, physx::PxTransform { physx::PxIdentity }, rActor, physx::PxTransform { physx::PxIdentity });
                rHandJoint->setLinearLimit(physx::PxJointLinearLimit{physx::PxTolerancesScale{}, 1.25f});
                rHandJoint->setMotion(physx::PxD6Axis::eX, physx::PxD6Motion::eLIMITED);
                rHandJoint->setMotion(physx::PxD6Axis::eY, physx::PxD6Motion::eLIMITED);
                rHandJoint->setMotion(physx::PxD6Axis::eZ, physx::PxD6Motion::eLIMITED);
                rHandJoint->setMotion(physx::PxD6Axis::eSWING1, physx::PxD6Motion::eFREE);
                rHandJoint->setMotion(physx::PxD6Axis::eSWING2, physx::PxD6Motion::eFREE);
                rHandJoint->setMotion(physx::PxD6Axis::eTWIST, physx::PxD6Motion::eFREE);
            }
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

        enet_deinitialize();
    }
}
