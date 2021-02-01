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
        LocospherePlayerComponent()
            : isLocal{true}
            , xzMoveInput{0.0f} {}
        bool isLocal;
        float maxSpeed;
        glm::vec2 xzMoveInput;
        bool sprint;
        bool grounded;
        bool jump;
        bool doubleJumpUsed = false;
    };

    struct PlayerRig {
        PlayerRig()
            : locosphere {entt::null}
            , fender {entt::null}
            , lHand {entt::null}
            , rHand {entt::null}
            , fenderJoint {nullptr} {}
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
        void onPlayerConstruct(entt::registry& reg, entt::entity ent);
        void onPlayerDestroy(entt::registry& reg, entt::entity ent);
        glm::vec3 calcHeadbobPosition(glm::vec3 desiredVel, glm::vec3 camPos, float deltaTime, bool grounded);
        worlds::IVRInterface* vrInterface;
        worlds::InputManager* inputManager;
        entt::registry& registry;
        worlds::Camera* camera;
        glm::vec3 lastCamPos;
        glm::vec3 nextCamPos;
        V3PidController lspherePid;
        float zeroThresh;

        float headbobProgress;

        float lookX;
        float lookY;
    };
}
