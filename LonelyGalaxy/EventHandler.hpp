#pragma once
#include "Physics/Physics.hpp"
#define NOMINMAX
#include <enet/enet.h>
#include <Core/IGameEventHandler.hpp>
#include <Core/Console.hpp>
#include <Core/Engine.hpp>
#include "NetMessage.hpp"
#include "PidController.hpp"
#include <Render/Camera.hpp>
#include "LocospherePlayerSystem.hpp"
#include "Client.hpp"
#include "Server.hpp"
#include <deque>
#include "MultiplayerManager.hpp"

namespace worlds {
    typedef uint64_t InputActionHandle;
}

namespace lg {
    class PlayerGrabManager;
    class EventHandler : public worlds::IGameEventHandler {
    public:
        EventHandler(bool dedicatedServer);
        void init(entt::registry& registry, worlds::EngineInterfaces interfaces) override;
        void preSimUpdate(entt::registry& registry, float deltaTime) override;
        void update(entt::registry& registry, float deltaTime, float interpAlpha) override;
        void simulate(entt::registry& registry, float simStep) override;
        void onSceneStart(entt::registry& reg) override;
        void shutdown(entt::registry& registry) override;
    private:
        void updateHandGrab(entt::registry& registry, PlayerRig& rig, entt::entity handEnt, float deltaTime);
        void onPhysicsSoundConstruct(entt::registry& reg, entt::entity ent);
        void onPhysicsSoundContact(entt::entity thisEnt, const worlds::PhysicsContactInfo& info);

        worlds::IVRInterface* vrInterface;
        worlds::VKRenderer* renderer;
        worlds::InputManager* inputManager;
        worlds::Camera* camera;
        worlds::WorldsEngine* engine;
        LocospherePlayerSystem* lsphereSys;
        entt::registry* reg;
        bool isDedicated;
        Client* client;
        Server* server;
        MultiplayerManager* mpManager;
        entt::entity lHandEnt = entt::null;
        entt::entity rHandEnt = entt::null;
        physx::PxD6Joint* lHandJoint, *rHandJoint;
        bool setClientInfo = false;
        PlayerGrabManager* playerGrabManager = nullptr;
        entt::entity audioListenerEntity = entt::null;

        worlds::InputActionHandle rStick;
    };
}
