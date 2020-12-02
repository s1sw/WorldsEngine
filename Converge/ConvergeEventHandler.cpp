#include "ConvergeEventHandler.hpp"
#include <openvr.h>
#include <Log.hpp>
#include <Render.hpp>
#include "AssetDB.hpp"
#include "DebugArrow.hpp"
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

namespace converge {
    const int MAX_PLAYERS = 32;
    const int CONVERGE_PORT = 3011;

    void cmdToggleVsync(void* obj, const char*) {
        auto renderer = (worlds::VKRenderer*)obj;
        renderer->setVsync(!renderer->getVsync());
    }

    EventHandler::EventHandler(bool dedicatedServer) 
        : isDedicated{dedicatedServer}
        , enetHost{nullptr} {
    }

    void EventHandler::init(entt::registry& registry, worlds::EngineInterfaces interfaces) {
        vrInterface = interfaces.vrInterface;
        renderer = interfaces.renderer;
        camera = interfaces.mainCamera;
        inputManager = interfaces.inputManager;

        worlds::g_console->registerCommand(cmdToggleVsync, "r_toggleVsync", "Toggles Vsync.", renderer);
        lsphereSys = new LocospherePlayerSystem { interfaces, registry };
        interfaces.engine->addSystem(lsphereSys);
        interfaces.engine->addSystem(new PhysHandSystem{ interfaces, registry });

        if (enet_initialize() != 0) {
            logErr("Failed to initialize enet.");
        }

        if (isDedicated) {
            ENetAddress address;
            address.host = ENET_HOST_ANY;
            address.port = 3011;
            enetHost = enet_host_create(&address, 32, 2, 0, 0);
            if (enetHost == NULL) {
                logErr("An error occurred while trying to create an ENet server host.");
                exit (EXIT_FAILURE);
            }
        } else {
            enetHost = enet_host_create(nullptr, 1, 2, 0, 0);
        }

        worlds::g_console->registerCommand([&](void*, const char* arg) {
            if (isDedicated) {
                logErr("this is a server! what are you trying to do???");
                return;
            }

            }, "cnvrg_connect", "Connects to the specified server.", nullptr);

        new DebugArrows(registry);

        // create... a SECOND LOCOSPHERE :O
        PlayerRig other = lsphereSys->createPlayerRig(registry);
        auto& lpc = registry.get<LocospherePlayerComponent>(other.locosphere);
        lpc.isLocal = false;
        lpc.sprint = false;
        lpc.maxSpeed = 0.0f;
        lpc.xzMoveInput = glm::vec2(0.0f, 0.0f);
        otherLocosphere = other.locosphere;

        auto sphereMeshId = worlds::g_assetDB.addOrGetExisting("uvsphere.obj");
        auto sphereMatId = worlds::g_assetDB.addOrGetExisting("Materials/dev.json");
        registry.emplace<worlds::WorldObject>(other.locosphere, sphereMatId, sphereMeshId);

        worlds::g_console->executeCommandStr("exec dbgscripts/shapes");

        auto errorMesh = worlds::g_assetDB.addOrGetExisting("srcerr/error.mdl");
        auto errorMat = worlds::g_assetDB.addOrGetExisting("Materials/error.json");

        error = worlds::createModelObject(registry, glm::vec3{0.0f}, glm::quat{}, errorMesh, errorMat);
    }

    void EventHandler::preSimUpdate(entt::registry& registry, float deltaTime) {
        
    }

    void EventHandler::update(entt::registry& registry, float deltaTime, float interpAlpha) {
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

        if (!registry.valid(localLocosphereEnt))
            logWarn("couldn't find a local locosphere!");

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

        registry.get<Transform>(error).position = t2.position + glm::vec3 {0.0f, 3.0f, 0.0f};
        g_dbgArrows->newFrame();
    }

    void EventHandler::simulate(entt::registry& registry, float simStep) {
        
    }

    void EventHandler::onSceneStart(entt::registry& registry) {
        g_dbgArrows->createEntities();
    }

    void EventHandler::shutdown(entt::registry& registry) {
        if (enetHost)
            enet_host_destroy(enetHost);
        enet_deinitialize();
    }
}
