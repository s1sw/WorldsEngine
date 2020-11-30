#include "ConvergeEventHandler.hpp"
#include <openvr.h>
#include <Log.hpp>
#include <Render.hpp>
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

namespace converge {
    const int MAX_PLAYERS = 32;
    const int CONVERGE_PORT = 3011;

    void loadControllerRenderModel(const char* name, entt::entity ent, entt::registry& reg, worlds::VKRenderer* renderer) {
        if (!reg.valid(ent) || reg.has<worlds::ProceduralObject>(ent) || name == nullptr) return;

        vr::RenderModel_t* model;
        auto err = vr::VRRenderModels()->LoadRenderModel_Async(name, &model);
        if (err != vr::VRRenderModelError_None)
            return;

        if (model == nullptr) {
            logErr(worlds::WELogCategoryEngine, "SteamVR render model was null");
            return;
        }

        if (model->unVertexCount == 0 || model->unTriangleCount == 0) {
            logErr(worlds::WELogCategoryEngine, "SteamVR render model was invalid");
            return;
        }

        worlds::ProceduralObject& procObj = reg.emplace<worlds::ProceduralObject>(ent);
        procObj.material = worlds::g_assetDB.addOrGetExisting("Materials/dev.json");

        procObj.vertices.reserve(model->unVertexCount);

        for (uint32_t i = 0; i < model->unVertexCount; i++) {
            vr::RenderModel_Vertex_t vrVert = model->rVertexData[i];
            worlds::Vertex vert;
            vert.position = glm::vec3(vrVert.vPosition.v[0], vrVert.vPosition.v[1], vrVert.vPosition.v[2]);
            vert.normal = glm::vec3(vrVert.vNormal.v[0], vrVert.vNormal.v[1], vrVert.vNormal.v[2]);
            vert.uv = glm::vec2(vrVert.rfTextureCoord[0], vrVert.rfTextureCoord[1]);
            procObj.vertices.push_back(vert);
        }

        procObj.indices.reserve(model->unTriangleCount * 3);
        for (unsigned int i = 0; i < model->unTriangleCount * 3; i++) {
            procObj.indices.push_back(model->rIndexData[i]);
        }

        procObj.indexType = vk::IndexType::eUint32;
        procObj.readyForUpload = true;
        procObj.dbgName = name;

        renderer->uploadProcObj(procObj);

        logMsg("Loaded SteamVR render model %s with %u vertices and %u triangles", name, model->unVertexCount, model->unTriangleCount);
    }

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
        interfaces.engine->addSystem(new LocospherePlayerSystem{interfaces, registry});
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
    }

    void EventHandler::preSimUpdate(entt::registry& registry, float deltaTime) {
        
    }

    void EventHandler::update(entt::registry& registry, float deltaTime, float interpAlpha) {
        
    }

    void EventHandler::simulate(entt::registry& registry, float simStep) {
        
    }

    void EventHandler::onSceneStart(entt::registry& registry) {
        
    }

    void EventHandler::shutdown(entt::registry& registry) {
        if (enetHost)
            enet_host_destroy(enetHost);
        enet_deinitialize();
    }
}
