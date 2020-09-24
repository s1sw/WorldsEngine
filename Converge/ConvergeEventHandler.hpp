#pragma once
#include <IGameEventHandler.hpp>
#include <Console.hpp>

namespace converge {
    class EventHandler : public worlds::IGameEventHandler {
    public:
        EventHandler();
        void init(entt::registry& registry, worlds::EngineInterfaces interfaces) override;
        void preSimUpdate(entt::registry& registry, float deltaTime) override;
        void update(entt::registry& registry, float deltaTime, float interpAlpha) override;
        void simulate(entt::registry& registry, float simStep) override;
        void onSceneStart(entt::registry& reg);
        void shutdown(entt::registry& registry);
    private:
        worlds::IVRInterface* vrInterface;
        worlds::VKRenderer* renderer;
        InputManager* inputManager;
        entt::entity lHandEnt, rHandEnt;
        char* lHandRMName;
        char* rHandRMName;
        Camera* camera;
        entt::entity playerLocosphere;
        entt::entity playerFender;
        entt::entity playerHead;
        bool jumpThisFrame;
        glm::vec3 lastCamPos;
        glm::vec3 nextCamPos;
    };
}