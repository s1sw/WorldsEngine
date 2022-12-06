#include "Engine.hpp"
#include <Core/Console.hpp>
#include <Physics/Physics.hpp>
#include <Scripting/NetVM.hpp>
#include <robin_hood.h>
#include <Tracy.hpp>
#include <Util/TimingUtil.hpp>

namespace worlds
{
    robin_hood::unordered_map<entt::entity, physx::PxTransform> currentState;
    robin_hood::unordered_map<entt::entity, physx::PxTransform> previousState;

    ConVar pauseSimulation  { "sim_pause", "0",
                              "Pauses the simulation (physics and Simulate() methods)." };
    ConVar lockSimToRefresh { "sim_lockToRefresh", "0",
                              "Instead of using a simulation timestep, run the simulation in "
                              "lockstep with the rendering." };
    ConVar disableSimInterp { "sim_disableInterp", "0",
                              "Disables interpolation and uses the results of the last run "
                              "simulation step." };
    ConVar simStepTime      { "sim_stepTime", "0.01",
                              "Time between each simulation step in seconds (as long as "
                              "sim_lockToRefresh is 0)." };

    SimulationLoop::SimulationLoop(const EngineInterfaces& interfaces, IGameEventHandler* evtHandler,
                                   entt::registry& registry)
        : physics(interfaces.physics)
        , scriptEngine(interfaces.scriptEngine)
        , registry(registry)
        , evtHandler(evtHandler)
    {
    }

    void SimulationLoop::doSimStep(float deltaTime, bool physicsOnly)
    {
        ZoneScoped;

        if (!physicsOnly)
        {
            evtHandler->simulate(registry, deltaTime);

            scriptEngine->onSimulate(deltaTime);
        }

        physics->stepSimulation(deltaTime);
    }


    bool SimulationLoop::updateSimulation(float& interpAlpha, double timeScale,
                                          double deltaTime, bool physicsOnly)
    {
        ZoneScoped;
        bool ran = false;

        if (pauseSimulation) return false;

        if (lockSimToRefresh.getInt() || disableSimInterp.getInt())
        {
            registry.view<RigidBody, Transform>().each(
                    [](RigidBody& dpa, Transform& transform)
                    {
                        auto curr = dpa.actor->getGlobalPose();

                        if (curr.p != glm2px(transform.position) || curr.q != glm2px(transform.rotation))
                        {
                            physx::PxTransform pt(glm2px(transform.position), glm2px(transform.rotation));
                            dpa.actor->setGlobalPose(pt);
                        }
                    }
            );
        }

        registry.view<PhysicsActor, Transform>().each(
                [](PhysicsActor& pa, Transform& transform)
                {
                    auto curr = pa.actor->getGlobalPose();
                    if (curr.p != glm2px(transform.position) || curr.q != glm2px(transform.rotation))
                    {
                        physx::PxTransform pt(glm2px(transform.position), glm2px(transform.rotation));
                        pa.actor->setGlobalPose(pt);
                    }
                }
        );

        if (!lockSimToRefresh.getInt())
        {
            simAccumulator += deltaTime;

            if (registry.view<RigidBody>().size() != currentState.size())
            {
                currentState.clear();
                previousState.clear();

                currentState.reserve(registry.view<RigidBody>().size());
                previousState.reserve(registry.view<RigidBody>().size());

                registry.view<RigidBody>().each(
                        [&](auto ent, RigidBody& dpa)
                        {
                            auto startTf = dpa.actor->getGlobalPose();
                            currentState.insert({ent, startTf});
                            previousState.insert({ent, startTf});
                        }
                );
            }

            while (simAccumulator >= simStepTime.getFloat())
            {
                ran = true;
                ZoneScopedN("Simulation step");
                previousState = currentState;
                simAccumulator -= simStepTime.getFloat();

                PerfTimer timer;

                doSimStep(simStepTime.getFloat() * timeScale, physicsOnly);

                double realTime = timer.stopGetMs() / 1000.0;

                // avoid spiral of death if simulation is taking too long
                if (realTime > simStepTime.getFloat())
                    simAccumulator = 0.0;
            }

            registry.view<RigidBody>().each([&](auto ent, RigidBody& dpa)
                                            { currentState[ent] = dpa.actor->getGlobalPose(); });

            float alpha = simAccumulator / simStepTime.getFloat();

            if (disableSimInterp.getInt() || simStepTime.getFloat() < deltaTime)
                alpha = 1.0f;

            registry.view<RigidBody, Transform>().each(
                    [&](entt::entity ent, RigidBody& dpa, Transform& transform)
                    {
                        if (!previousState.contains(ent))
                        {
                            transform.position = px2glm(currentState[ent].p);
                            transform.rotation = px2glm(currentState[ent].q);
                        }
                        else
                        {
                            transform.position =
                                    glm::mix(px2glm(previousState[ent].p), px2glm(currentState[ent].p), (float)alpha);
                            transform.rotation =
                                    glm::slerp(px2glm(previousState[ent].q), px2glm(currentState[ent].q), (float)alpha);
                        }
                    }
            );

            registry.view<RigidBody, ChildComponent, Transform>().each(
                    [&](entt::entity ent, RigidBody& dpa, ChildComponent& cc, Transform& t)
                    {
                        cc.offset = t.transformByInverse(registry.get<Transform>(cc.parent));
                    }
            );
            interpAlpha = alpha;
        }
        else if (deltaTime < 0.05f)
        {
            doSimStep(deltaTime, physicsOnly);
            ran = true;

            registry.view<RigidBody, Transform>().each(
                    [&](entt::entity, RigidBody& dpa, Transform& transform)
                    {
                        transform.position = px2glm(dpa.actor->getGlobalPose().p);
                        transform.rotation = px2glm(dpa.actor->getGlobalPose().q);
                    }
            );
            registry.view<RigidBody, ChildComponent, Transform>().each(
                    [&](entt::entity ent, RigidBody& dpa, ChildComponent& cc, Transform& t)
                    {
                        cc.offset = t.transformByInverse(registry.get<Transform>(cc.parent));
                    }
            );
        }

        return ran;
    }
}