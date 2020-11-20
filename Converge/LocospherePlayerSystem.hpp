#pragma once
#include <entt/entt.hpp>
#include <IGameEventHandler.hpp>
#include "PidController.hpp"
#include "ISystem.hpp"
#include <IVRInterface.hpp>
#include <Camera.hpp>

namespace converge {
    class LocospherePlayerSystem : public worlds::ISystem {
    public:
        LocospherePlayerSystem(worlds::EngineInterfaces interfaces, entt::registry& registry);
        void onSceneStart(entt::registry& registry) override;
        void update(entt::registry& registry, float deltaTime, float interpAlpha) override;
        void preSimUpdate(entt::registry& registry, float deltaTime) override;
        void simulate(entt::registry& registry, float simStep) override;
        void shutdown(entt::registry& registry) override;
    private:
        worlds::IVRInterface* vrInterface;
        worlds::VKRenderer* renderer;
        worlds::InputManager* inputManager;
        entt::registry& registry;
        worlds::Camera* camera;
        entt::entity lHandEnt, rHandEnt;
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
        worlds::InputActionHandle grappleHookAction;
        V3PidController lspherePid;
        float zeroThresh;

        float headbobProgress;
        bool grounded;
    };
}