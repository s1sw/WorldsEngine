#pragma once
#include <entt/entt.hpp>
#include "PhysicsActor.hpp"
#include "PidController.hpp"
#include <IVRInterface.hpp>
#include <Transform.hpp>
#include <Physics.hpp>

namespace converge {
    class PhysHandController {
    public:
        static const int DEBUG_BUFFER_LEN = 128;

        PhysHandController(
            entt::entity handEnt, 
            const PIDSettings& posSettings, 
            const PIDSettings& rotSettings,
            entt::registry& registry);

        void setTargetTransform(const Transform& t);
        void applyForces(float simStep);
        void drawImGuiDebug();

        const float* getTorqueDbg() { return torqueMag; }
        const float* getForceDbg() { return forceMag; }
        int getCurrDbgIdx() { return currDbgIdx; }
    private:
        physx::PxRigidBody* body;
        entt::entity handEnt;
        V3PidController posPid;
        V3PidController rotPid;
        glm::vec3 targetPos;
        glm::quat targetRot;
        entt::registry& registry;

        float torqueMag[DEBUG_BUFFER_LEN];
        float forceMag[DEBUG_BUFFER_LEN];
        int currDbgIdx = 0;
    };
}
