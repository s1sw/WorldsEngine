#include "EventHandler.hpp"
#include <Core/Engine.hpp>
#include <Render/Render.hpp>
#include "DebugArrow.hpp"

namespace lg {
    void cmdToggleVsync(void* obj, const char*) {
        auto renderer = (worlds::Renderer*)obj;
        renderer->setVsync(!renderer->getVsync());
    }

    EventHandler::EventHandler(bool dedicatedServer) {
    }

    void EventHandler::init(entt::registry& registry, worlds::EngineInterfaces interfaces) {
        this->interfaces = interfaces;
        vrInterface = interfaces.vrInterface;
        renderer = interfaces.renderer;
        camera = interfaces.mainCamera;
        inputManager = interfaces.inputManager;
        engine = interfaces.engine;
        reg = &registry;

        worlds::g_console->registerCommand(cmdToggleVsync, "r_toggleVsync", "Toggles Vsync.", renderer);

        new DebugArrows(registry);
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
