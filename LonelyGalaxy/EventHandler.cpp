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
#include <Core/JobSystem.hpp>
#include "ObjectParentSystem.hpp"
#include "Util/VKImGUIUtil.hpp"
#include "MathsUtil.hpp"
#include <Input/Input.hpp>
#include <Audio/Audio.hpp>
#include <UI/WorldTextComponent.hpp>
#include <Serialization/SceneSerialization.hpp>
#include <Libs/pcg_basic.h>
#include <Core/Window.hpp>

namespace lg {
    void cmdToggleVsync(void* obj, const char*) {
        auto renderer = (worlds::Renderer*)obj;
        renderer->setVsync(!renderer->getVsync());
    }

    EventHandler::EventHandler(bool dedicatedServer) {
    }

    entt::entity camcorder = entt::null;

    void EventHandler::init(entt::registry& registry, worlds::EngineInterfaces interfaces) {
        this->interfaces = interfaces;
        vrInterface = interfaces.vrInterface;
        renderer = interfaces.renderer;
        camera = interfaces.mainCamera;
        inputManager = interfaces.inputManager;
        engine = interfaces.engine;
        reg = &registry;

        worlds::g_console->registerCommand(cmdToggleVsync, "r_toggleVsync", "Toggles Vsync.", renderer);
        interfaces.engine->addSystem(new ObjectParentSystem);

        new DebugArrows(registry);

        worlds::g_console->registerCommand([&](void*, const char*) {
            camcorder = worlds::SceneLoader::createPrefab(worlds::AssetDB::pathToId("Prefabs/spectator_camcorder.wprefab"), registry);
            }, "lg_spawnCamcorder", "Spawns the camcorder.", nullptr);
    }

    void EventHandler::preSimUpdate(entt::registry&, float) {
        g_dbgArrows->newFrame();
    }

    void EventHandler::update(entt::registry& reg, float deltaTime, float) {
    }

    void EventHandler::simulate(entt::registry& registry, float simStep) {
    }

    void EventHandler::onSceneStart(entt::registry& registry) {
        g_dbgArrows->createEntities();
        camera->rotation = glm::quat{ glm::vec3{0.0f} };
    }

    void EventHandler::shutdown(entt::registry& registry) {
    }
}
