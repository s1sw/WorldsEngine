#pragma once
#include <IGameEventHandler.hpp>
#include <Console.hpp>
#include "PidController.hpp"

namespace converge {
    class EventHandler : public worlds::IGameEventHandler {
    public:
        EventHandler();
        void init(entt::registry& registry, worlds::EngineInterfaces interfaces) override;
        void preSimUpdate(entt::registry& registry, float deltaTime) override;
        void update(entt::registry& registry, float deltaTime, float interpAlpha) override;
        void simulate(entt::registry& registry, float simStep) override;
        void onSceneStart(entt::registry& reg) override;
        void shutdown(entt::registry& registry) override;
    private:
        worlds::IVRInterface* vrInterface;
        worlds::VKRenderer* renderer;
        InputManager* inputManager;
        entt::entity lHandEnt, rHandEnt;
        Camera* camera;
        entt::entity playerLocosphere;
        entt::entity playerFender;
        bool jumpThisFrame;
        glm::vec3 lastCamPos;
        glm::vec3 nextCamPos;
        V3PidController lHandPid;
        V3PidController rHandPid;
        V3PidController lHandRotPid;
        V3PidController rHandRotPid;
        glm::vec3 lHandWPos;
        glm::vec3 rHandWPos;
        glm::quat lHandWRot;
        glm::quat rHandWRot;
        worlds::InputActionHandle throwHandAction;
        V3PidController lspherePid;
        float zeroThresh;

        float headbobProgress;
        bool grounded;
    };
}