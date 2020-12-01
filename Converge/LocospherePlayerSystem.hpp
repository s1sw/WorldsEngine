#pragma once
#include <entt/entt.hpp> 
#include <IGameEventHandler.hpp>
#include "PidController.hpp"
#include "ISystem.hpp"
#include <IVRInterface.hpp>
#include <Camera.hpp>
#include "PhysHandSystem.hpp"

namespace converge {
    struct HeadBobSettings {
        HeadBobSettings()
            : bobSpeed { 7.5f, 15.0f }
            , bobAmount{ 0.1f, 0.05f }
            , overallSpeed { 1.0f }
            , sprintMult { 1.25f } {}
        glm::vec2 bobSpeed;
        glm::vec2 bobAmount;
        float overallSpeed;
        float sprintMult;
    };

    struct LocospherePlayerComponent {
        bool isLocal;
        float maxSpeed;
        glm::vec2 xzMoveInput;
        bool sprint;
        bool grounded;
    };

    struct PlayerRig {
        entt::entity locosphere;
        entt::entity fender;
        entt::entity lHand;
        entt::entity rHand;
        physx::PxD6Joint* fenderJoint;
    };

    class LocospherePlayerSystem : public worlds::ISystem {
    public:
        LocospherePlayerSystem(worlds::EngineInterfaces interfaces, entt::registry& registry);
        void onSceneStart(entt::registry& registry) override;
        void update(entt::registry& registry, float deltaTime, float interpAlpha) override;
        void preSimUpdate(entt::registry& registry, float deltaTime) override;
        void simulate(entt::registry& registry, float simStep) override;
        void shutdown(entt::registry& registry) override;
        PlayerRig createPlayerRig(entt::registry& registry);
    private:
        void updateHands(entt::registry& reg);
        void onPlayerConstruct(entt::registry& reg, entt::entity ent);
        void onPlayerDestroy(entt::registry& reg, entt::entity ent);
        glm::vec3 calcHeadbobPosition(glm::vec3 desiredVel, glm::vec3 camPos, float deltaTime, bool grounded);
        worlds::IVRInterface* vrInterface;
        worlds::InputManager* inputManager;
        entt::registry& registry;
        worlds::Camera* camera;
        entt::entity lHandEnt, rHandEnt;
        entt::entity grappleIndicator;
        bool jumpThisFrame;
        glm::vec3 lastCamPos;
        glm::vec3 nextCamPos;
        worlds::InputActionHandle grappleHookAction;
        V3PidController lspherePid;
        float zeroThresh;

        float headbobProgress;

        float lookX;
        float lookY;
    };
}
