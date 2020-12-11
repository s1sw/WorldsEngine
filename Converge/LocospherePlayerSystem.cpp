#include "LocospherePlayerSystem.hpp"
#include <Physics.hpp>
#include <imgui.h>
#include <openvr.h>
#include <OpenVRInterface.hpp>
#include <NameComponent.hpp>
#include <Engine.hpp>
#include <Console.hpp>
#include <Input.hpp>
#include <Camera.hpp>
#include "MathsUtil.hpp"
#include <CreateModelObject.hpp>
#include "DebugArrow.hpp"

namespace converge {
    class NullPhysXCallback : public physx::PxRaycastCallback {
    public:
        NullPhysXCallback() : physx::PxRaycastCallback{ nullptr, 0 } {

        }

        physx::PxAgain processTouches(const physx::PxRaycastHit*, physx::PxU32) override {
            return false;
        }
    };

    entt::entity getActorEntity(physx::PxRigidActor* actor) {
        return (entt::entity)(uint32_t)(uintptr_t)actor->userData;
    }

    struct FilterEntity : public physx::PxQueryFilterCallback {
        entt::entity entA;
        entt::entity entB;

        physx::PxQueryHitType::Enum preFilter(const physx::PxFilterData&, const physx::PxShape*, const physx::PxRigidActor*, physx::PxHitFlags&) override {
            return physx::PxQueryHitType::eBLOCK;
        }


        physx::PxQueryHitType::Enum postFilter(const physx::PxFilterData&, const physx::PxQueryHit& hit) override {
            if (getActorEntity(hit.actor) == entA || getActorEntity(hit.actor) == entB) {
                return physx::PxQueryHitType::eNONE;
            }
            return physx::PxQueryHitType::eBLOCK;
        }
    };

    const float LOCOSPHERE_RADIUS = 0.25f;

    physx::PxMaterial* locosphereMat;
    physx::PxMaterial* fenderMat;
    physx::PxD6Joint* lHandJoint;
    physx::PxD6Joint* rHandJoint;

    worlds::ConVar showLocosphereDebug{ "cnvrg_showLocosphereDebug", "0", "Shows the locosphere debug menu." };
    worlds::ConVar drawDbgArrows { "cnvrg_locoDbgArrow", "0", "Draw a debug arrow pointing in the current input direction." };

    glm::vec3 lastDesiredVel;

    int physDbgIdx = 0;

    worlds::ConVar overallAmount{ "cnvrg_headBobAmount", "0.5" };
    worlds::ConVar headBobDbg{ "cnvrg_headBobDebug", "0" };
    worlds::ConVar speedometer{ "cnvrg_speedometer", "0" };
    worlds::ConVar mouseSensitivity { "cnvrg_mouseSensitivity", "1.0" };

    struct LocosphereDebugInfo {
        float angVels[128];
        float linVels[128];
        float linVelsXZ[128];
    };

    std::unordered_map<entt::entity, LocosphereDebugInfo> locosphereDebug;

    LocospherePlayerSystem::LocospherePlayerSystem(worlds::EngineInterfaces interfaces, entt::registry& registry)
        : vrInterface{ interfaces.vrInterface }
        , inputManager{ interfaces.inputManager }
        , registry{ registry }
        , camera{ interfaces.mainCamera }
        , lookX{ 0.0f }
        , lookY{ 0.0f } {
        auto onConstruct = registry.on_construct<LocospherePlayerComponent>();
        onConstruct.connect<&LocospherePlayerSystem::onPlayerConstruct>(*this);

        auto onDestroy = registry.on_destroy<LocospherePlayerComponent>();
        onDestroy.connect<&LocospherePlayerSystem::onPlayerDestroy>(*this);
    }

    void LocospherePlayerSystem::onPlayerConstruct(entt::registry&, entt::entity ent) {
        locosphereDebug.insert({ ent, LocosphereDebugInfo {} });
    }

    void LocospherePlayerSystem::onPlayerDestroy(entt::registry&, entt::entity ent) {
        locosphereDebug.erase(ent);
    }

    void LocospherePlayerSystem::preSimUpdate(entt::registry& reg, float) {
        entt::entity localLocosphereEnt = entt::null;

        reg.view<LocospherePlayerComponent>().each([&](auto ent, auto& lpc) {
            if (lpc.isLocal) {
                if (!reg.valid(localLocosphereEnt))
                    localLocosphereEnt = ent;
                else {
                    logWarn("more than one local locosphere!");
                }
            }
        });

        if (!reg.valid(localLocosphereEnt)) {
            return;
        }

        auto& lpc = reg.get<LocospherePlayerComponent>(localLocosphereEnt);
        if (!vrInterface) {
            lpc.jump |= inputManager->keyPressed(SDL_SCANCODE_SPACE);
        } else {
            lpc.jump |= vrInterface->getJumpInput();
        }
    }

    glm::vec3 LocospherePlayerSystem::calcHeadbobPosition(glm::vec3 desiredVel, glm::vec3 camPos, float deltaTime, bool grounded) {
        static float headbobTime = 0.0f;
        headbobTime += deltaTime;

        static HeadBobSettings settings;

        if (headBobDbg && ImGui::Begin("Head bob")) {
            ImGui::InputFloat("Speed Y", &settings.bobSpeed.x);
            ImGui::InputFloat("Speed X", &settings.bobSpeed.y);
            ImGui::InputFloat("Amount Y", &settings.bobAmount.y);
            ImGui::InputFloat("Amount X", &settings.bobAmount.x);
            ImGui::InputFloat("Overall Speed", &settings.overallSpeed);
        }

        static float sprintLerp = 0.0f;

        if (glm::length2(desiredVel) > 0.0f && grounded) {
            headbobProgress += deltaTime;
        }

        if (inputManager->keyHeld(SDL_SCANCODE_LSHIFT)) {
            sprintLerp += deltaTime * 5.0f;
        } else {
            sprintLerp -= deltaTime * 5.0f;
        }

        sprintLerp = glm::clamp(sprintLerp, 0.0f, 1.0f);

        glm::vec2 bobSpeed = settings.bobSpeed * settings.overallSpeed;
        glm::vec2 bobAmount = settings.bobAmount * overallAmount.getFloat();

        glm::vec3 bobbedPosNormal = camPos;
        glm::vec3 bobbedPosSprint = camPos;

        bobbedPosNormal.y += glm::sin(headbobProgress * bobSpeed.y) * bobAmount.y;

        glm::vec3 xBob{
            glm::sin(headbobProgress * bobSpeed.x), 
            0.0f, 
            0.0f 
        };

        xBob *= bobAmount.x;
        xBob = xBob * glm::angleAxis(lookX, glm::vec3(0.0f, 1.0f, 0.0f));

        bobbedPosNormal += xBob;

        // repeat the above but with sprinting multiplier to get
        // the head position bobbed for sprinting
        bobSpeed *= settings.sprintMult;
        bobAmount *= settings.sprintMult;

        bobbedPosSprint.y += glm::sin(headbobProgress * bobSpeed.y) * bobAmount.y;

        glm::vec3 xBobS{
            glm::sin(headbobProgress * bobSpeed.x), 
            0.0f, 
            0.0f 
        };

        xBobS *= bobAmount.x;
        xBobS = xBobS * glm::angleAxis(lookX, glm::vec3(0.0f, 1.0f, 0.0f));

        bobbedPosSprint += xBobS;

        if (headBobDbg)
            ImGui::End();

        return glm::mix(bobbedPosNormal, bobbedPosSprint, sprintLerp);
    }

    void LocospherePlayerSystem::update(entt::registry& reg, float deltaTime, float interpAlpha) {
        entt::entity localLocosphereEnt = entt::null;

        reg.view<LocospherePlayerComponent>().each([&](auto ent, auto& lpc) {
            if (lpc.isLocal) {
                if (!reg.valid(localLocosphereEnt))
                    localLocosphereEnt = ent;
                else {
                    logWarn("more than one local locosphere!");
                }
            }
        });

        if (!reg.valid(localLocosphereEnt)) {
            return;
        }

        if (!vrInterface) {

            lookX += (float)(inputManager->getMouseDelta().x) * 0.005f * mouseSensitivity.getFloat();
            lookY += (float)(inputManager->getMouseDelta().y) * 0.005f * mouseSensitivity.getFloat();

            lookY = glm::clamp(lookY, -glm::half_pi<float>() + 0.001f, glm::half_pi<float>() - 0.001f);

            camera->rotation = glm::angleAxis(-lookX, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::angleAxis(lookY, glm::vec3(1.0f, 0.0f, 0.0f));
        }

        camera->position = glm::mix(lastCamPos, nextCamPos, interpAlpha);

        glm::vec3 desiredVel{ 0.0f };

        if (!vrInterface) {
            if (inputManager->keyHeld(SDL_SCANCODE_W)) {
                desiredVel.z += 1.0f;
            }

            if (inputManager->keyHeld(SDL_SCANCODE_S)) {
                desiredVel.z -= 1.0f;
            }

            if (inputManager->keyHeld(SDL_SCANCODE_A)) {
                desiredVel.x += 1.0f;
            }

            if (inputManager->keyHeld(SDL_SCANCODE_D)) {
                desiredVel.x -= 1.0f;
            }
        } else {
            vrInterface->updateInput();

            auto locIn = vrInterface->getLocomotionInput();
            // invert X to match coordinate system
            desiredVel = glm::vec3(-locIn.x, 0.0f, locIn.y);
        }

        if (glm::length2(desiredVel) > 0.0f)
            desiredVel = glm::normalize(desiredVel);

        glm::mat4 camMat;
        if (vrInterface) {
            camMat = vrInterface->getHeadTransform();
        } else {
            camMat = glm::rotate(glm::mat4(1.0f), -lookX, glm::vec3(0.0f, 1.0f, 0.0f));
        }

        desiredVel = camMat * glm::vec4(desiredVel, 0.0f);
        desiredVel.y = 0.0f;

        if (glm::length2(desiredVel) > 0.0f)
            desiredVel = glm::normalize(desiredVel);

        auto& localLpc = registry.get<LocospherePlayerComponent>(localLocosphereEnt);
        localLpc.xzMoveInput = glm::vec2 { desiredVel.x, desiredVel.z };

        localLpc.sprint = (vrInterface && vrInterface->getSprintInput()) || (inputManager->keyHeld(SDL_SCANCODE_LSHIFT));

        if (drawDbgArrows.getInt()) {
                auto& llstf = registry.get<Transform>(localLocosphereEnt);
            g_dbgArrows->drawArrow(llstf.position, desiredVel);
        }

        if (!vrInterface)
            camera->position = calcHeadbobPosition(desiredVel, camera->position, deltaTime, localLpc.grounded);

        if (showLocosphereDebug.getInt()) {
            bool s = true;
            if (ImGui::Begin("Locosphere", &s)) {
                auto& lDebugInfo = locosphereDebug.at(localLocosphereEnt);
                auto& lpc = localLpc;
                ImGui::Text("Grounded: %i", lpc.grounded);
                ImGui::PlotLines("Angular Speed", lDebugInfo.angVels, 128, physDbgIdx, nullptr, 0.0f, 50.0f, ImVec2(500.0f, 100.0f));
                ImGui::PlotLines("Linear Speed", lDebugInfo.linVels, 128, physDbgIdx, nullptr, 0.0f, 10.0f, ImVec2(500.0f, 100.0f));
                ImGui::PlotLines("Linear XZ Speed", lDebugInfo.linVelsXZ, 128, physDbgIdx, nullptr, 0.0f, 10.0f, ImVec2(500.0f, 100.0f));
                ImGui::Text("Input: %f, %f", lpc.xzMoveInput.x, lpc.xzMoveInput.y);

                float maxAngVel = 0.0f;
                float maxLinVel = 0.0f;
                float maxLinVelXZ = 0.0f;
                for (int i = 0; i < 128; i++) {
                    maxLinVel = std::max(lDebugInfo.linVels[i], maxLinVel);
                    maxLinVelXZ = std::max(lDebugInfo.linVelsXZ[i], maxLinVelXZ);
                    maxAngVel = std::max(lDebugInfo.angVels[i], maxAngVel);
                }
                ImGui::Text("Maximum angular speed: %.3f", maxAngVel);
                ImGui::Text("Maximum linear speed: %.3f", maxLinVel);
                ImGui::Text("Maximum linear speed (excluding Y): %.3f", maxLinVelXZ);

                int adjIdx = physDbgIdx - 1;
                if (adjIdx < 0) {
                    adjIdx = 128 - adjIdx;
                } else if (adjIdx > 127) {
                    adjIdx = adjIdx - 128;
                }

                ImGui::Text("Current angular speed: %.3f", lDebugInfo.angVels[adjIdx]);
                ImGui::Text("Current linear speed: %.3f", lDebugInfo.linVels[adjIdx]);
                ImGui::Text("Current linear speed (excluding Y): %.3f", lDebugInfo.linVelsXZ[adjIdx]);

                ImGui::DragFloat("P", &lspherePid.P);
                ImGui::DragFloat("I", &lspherePid.I);
                ImGui::DragFloat("D", &lspherePid.D);
                ImGui::DragFloat("Zero Thresh", &zeroThresh, 0.01f);
            }
            ImGui::End();

            if (!s)
                showLocosphereDebug.setValue("0");
        }


        if (speedometer) {
            auto size = ImGui::GetMainViewport()->Size;
            size.x *= 0.5f;
            size.y *= 0.75f;

            char buf[32];
            char buf2[32];

            int adjIdx = physDbgIdx - 1;
            if (adjIdx < 0) {
                adjIdx = 128 - adjIdx;
            } else if (adjIdx > 127) {
                adjIdx = adjIdx - 128;
            }

            auto& lld = locosphereDebug.at(localLocosphereEnt);

            sprintf(buf, "total vel: %.3fms^-1", lld.linVels[adjIdx]);
            sprintf(buf2, "xz vel: %.3fms^-1", lld.linVelsXZ[adjIdx]);

            auto& io = ImGui::GetIO();
            float fontSize = io.Fonts->Fonts[0]->FontSize;
            ImVec2 textSize = ImGui::CalcTextSize(buf);
            ImVec2 textSize2 = ImGui::CalcTextSize(buf2);

            ImGui::GetBackgroundDrawList()->AddText(ImVec2(size.x - textSize.x * 0.5f, size.y), ImColor(1.0f, 1.0f, 1.0f, 1.0f), buf);
            ImGui::GetBackgroundDrawList()->AddText(ImVec2(size.x - textSize2.x * 0.5f, size.y - fontSize), ImColor(1.0f, 1.0f, 1.0f, 1.0f), buf2);
        }

        // dot in the centre of the screen
        if (!vrInterface) {
            auto size = ImGui::GetMainViewport()->Size;
            ImGui::GetBackgroundDrawList()->AddCircleFilled(ImVec2(size.x * 0.5f, size.y * 0.5f), 2.5f, ImColor(1.0f, 1.0f, 1.0f), 16);
        }
    }

    glm::vec3 clampMagnitude(glm::vec3 v, float maxMagnitude) {
        float l2 = glm::length2(v);

        if (l2 > maxMagnitude * maxMagnitude) {
            v = glm::normalize(v) * maxMagnitude;
        }

        return v;
    }

    physx::PxVec3 clampMagnitude(physx::PxVec3 v, float maxMagnitude) {
        float l2 = v.magnitudeSquared();

        if (l2 > maxMagnitude * maxMagnitude) {
            v.normalize();
            v *= maxMagnitude;
        }

        return v;
    }

    void LocospherePlayerSystem::simulate(entt::registry& registry, float simStep) {
        registry.view<LocospherePlayerComponent>().each([&](auto ent, LocospherePlayerComponent& lpc) {
            auto& locosphereTransform = registry.get<Transform>(ent);

            const float maxSpeed = 15.0f;
            auto& wActor = registry.get<worlds::DynamicPhysicsActor>(ent);
            auto* locosphereActor = (physx::PxRigidDynamic*)wActor.actor;
            auto& rig = registry.get<PlayerRig>(ent);

            FilterEntity filterEnt;
            filterEnt.entA = ent;
            filterEnt.entB = rig.fender;

            glm::vec3 desiredVel(0.0f);
            
            desiredVel.x = lpc.xzMoveInput.x;
            desiredVel.z = lpc.xzMoveInput.y;

            desiredVel *= maxSpeed;

            if (lpc.sprint) {
                desiredVel *= 2.0f;
            }

            lastDesiredVel = desiredVel;

            // we now have a direction vector for travel
            // decompose it into torque components for the X and Z axis
            glm::vec3 desiredAngVel { 0.0f };
            desiredAngVel += glm::vec3 { 1.0f, 0.0f, 0.0f } * desiredVel.z;
            desiredAngVel += glm::vec3 { 0.0f, 0.0f, -1.0f } * desiredVel.x;

            auto currVel = worlds::px2glm(locosphereActor->getAngularVelocity());
            glm::vec3 torque = lspherePid.getOutput(desiredAngVel - currVel, simStep);
            locosphereActor->addTorque(worlds::glm2px(torque), physx::PxForceMode::eACCELERATION);

            if (locosphereActor->getLinearVelocity().magnitudeSquared() < zeroThresh * zeroThresh) {
                locosphereActor->setLinearVelocity(physx::PxVec3(0.0f, 0.0f, 0.0f));
            }

            NullPhysXCallback nullCallback{};
            lpc.grounded = worlds::g_scene->raycast(worlds::glm2px(locosphereTransform.position - glm::vec3(0.0f, LOCOSPHERE_RADIUS - 0.01f, 0.0f)), physx::PxVec3{ 0.0f, -1.0f, 0.0f }, LOCOSPHERE_RADIUS, nullCallback, physx::PxHitFlag::eDEFAULT, physx::PxQueryFilterData{ physx::PxQueryFlag::ePOSTFILTER | physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::eSTATIC }, &filterEnt);

            glm::vec3 currLinVel = worlds::px2glm(locosphereActor->getLinearVelocity());

            auto& lDebugInfo = locosphereDebug.at(ent);

            lDebugInfo.angVels[physDbgIdx] = glm::length(currVel);
            lDebugInfo.linVels[physDbgIdx] = glm::length(currLinVel);
            lDebugInfo.linVelsXZ[physDbgIdx] = glm::length(glm::vec3{ currLinVel.x, 0.0f, currLinVel.z });

            if (!lpc.grounded) {
                glm::vec3 airVel { lpc.xzMoveInput.x, 0.0f, lpc.xzMoveInput.y };
                airVel *= 25.0f;
                glm::vec3 addedVel = (airVel - currLinVel);
                addedVel.y = 0.0f;
                locosphereActor->addForce(worlds::glm2px(addedVel) * simStep, physx::PxForceMode::eACCELERATION);
            }

            if (lpc.jump) {
                if (lpc.grounded)
                    locosphereActor->addForce(physx::PxVec3{ 0.0f, 7.5f, 0.0f }, physx::PxForceMode::eVELOCITY_CHANGE);
                lpc.jump = false;
            }

            if (lpc.isLocal) {

                lastCamPos = nextCamPos;
                nextCamPos = locosphereTransform.position + glm::vec3(0.0f, -LOCOSPHERE_RADIUS, 0.0f);
            

                if (!vrInterface) {
                    // Make all non-VR users 1.75m tall
                    // Then subtract 5cm from that to account for eye offset
                    nextCamPos += glm::vec3(0.0f, 1.7f, 0.0f);
                }

                if (vrInterface) {
                    static glm::vec3 lastHeadPos{ 0.0f };
                    glm::vec3 headPos = worlds::getMatrixTranslation(vrInterface->getHeadTransform());
                    glm::vec3 locosphereOffset = lastHeadPos - headPos;
                    lastHeadPos = headPos;
                    locosphereOffset.y = 0.0f;

                    nextCamPos += glm::vec3(headPos.x, 0.0f, headPos.z);

                    physx::PxTransform locosphereptf = locosphereActor->getGlobalPose();
                    ImGui::Text("Locosphere Offset: %.3f, %.3f, %.3f", locosphereOffset.x, locosphereOffset.y, locosphereOffset.z);
                    locosphereptf.p += worlds::glm2px(locosphereOffset);
                    locosphereTransform.position += locosphereOffset;
                    locosphereActor->setGlobalPose(locosphereptf);

                    auto fenderActor = registry.get<worlds::DynamicPhysicsActor>(rig.fender).actor;
                    auto fenderWorldsTf = registry.get<Transform>(rig.fender);
                    physx::PxTransform fenderTf = fenderActor->getGlobalPose();
                    fenderTf.p += worlds::glm2px(locosphereOffset);
                    fenderWorldsTf.position += locosphereOffset;
                    fenderActor->setGlobalPose(fenderTf);
                }

                physDbgIdx++;
                if (physDbgIdx == 128)
                    physDbgIdx = 0;
            }
        });
    }

    PlayerRig LocospherePlayerSystem::createPlayerRig(entt::registry& registry) {
        // Locosphere
        auto playerLocosphere = registry.create();
        registry.emplace<Transform>(playerLocosphere).position = glm::vec3(0.0f, 2.0f, 0.0f);
        registry.emplace<LocospherePlayerComponent>(playerLocosphere).isLocal = true;

        auto actor = worlds::g_physics->createRigidDynamic(physx::PxTransform{ physx::PxVec3{0.0f, 2.0f, 0.0f}, physx::PxQuat{physx::PxIdentity} });
        auto& wActor = registry.emplace<worlds::DynamicPhysicsActor>(playerLocosphere, actor);

        actor->setSolverIterationCounts(30, 8);
        worlds::g_scene->addActor(*actor);

        locosphereMat = worlds::g_physics->createMaterial(2.5f, 50.0f, 0.0f);
        locosphereMat->setFrictionCombineMode(physx::PxCombineMode::eMAX);
        wActor.physicsShapes.push_back(worlds::PhysicsShape::sphereShape(LOCOSPHERE_RADIUS, locosphereMat));

        worlds::updatePhysicsShapes(wActor);
        physx::PxRigidBodyExt::setMassAndUpdateInertia(*actor, 80.0f);

        // Set up player fender and joint
        auto playerFender = registry.create();
        registry.emplace<Transform>(playerFender);

        auto fenderActor = worlds::g_physics->createRigidDynamic(physx::PxTransform{ physx::PxVec3{0.0f, 0.5f, 0.0f}, physx::PxQuat{physx::PxIdentity} });
        auto& fenderWActor = registry.emplace<worlds::DynamicPhysicsActor>(playerFender, fenderActor);

        worlds::g_scene->addActor(*fenderActor);

        fenderMat = worlds::g_physics->createMaterial(0.0f, 0.0f, 0.0f);
        auto fenderShape = worlds::PhysicsShape::capsuleShape(0.3f, 0.45f, fenderMat);
        fenderShape.rot = glm::quat(glm::vec3(0.0f, 0.0f, glm::half_pi<float>()));
        fenderWActor.physicsShapes.push_back(fenderShape);

        worlds::updatePhysicsShapes(fenderWActor);
        physx::PxRigidBodyExt::setMassAndUpdateInertia(*fenderActor, 8.0f);

        auto* fenderJoint = physx::PxD6JointCreate(*worlds::g_physics, fenderActor, physx::PxTransform{ physx::PxVec3{0.0f, -0.4f, 0.0f}, physx::PxQuat{ physx::PxIdentity } }, actor, physx::PxTransform{ physx::PxIdentity });

        fenderActor->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_X, true);
        fenderActor->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y, true);
        fenderActor->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z, true);

        fenderJoint->setMotion(physx::PxD6Axis::eX, physx::PxD6Motion::eLOCKED);
        fenderJoint->setMotion(physx::PxD6Axis::eY, physx::PxD6Motion::eLOCKED);
        fenderJoint->setMotion(physx::PxD6Axis::eZ, physx::PxD6Motion::eLOCKED);

        fenderJoint->setMotion(physx::PxD6Axis::eSWING1, physx::PxD6Motion::eFREE);
        fenderJoint->setMotion(physx::PxD6Axis::eSWING2, physx::PxD6Motion::eFREE);
        fenderJoint->setMotion(physx::PxD6Axis::eTWIST, physx::PxD6Motion::eFREE);

        fenderJoint->setConstraintFlag(physx::PxConstraintFlag::eCOLLISION_ENABLED, false);
        fenderActor->setSolverIterationCounts(30, 8);

        logMsg("locosphere entity is %u", (uint32_t)playerLocosphere);


        PlayerRig rig;
        rig.locosphere = playerLocosphere;
        rig.fender = playerFender;
        rig.fenderJoint = fenderJoint;
        registry.emplace<PlayerRig>(playerLocosphere, rig);
        
        return rig;
    }

    void LocospherePlayerSystem::onSceneStart(entt::registry&) {
        // Create physics rig
        lspherePid.P = 50.0f;
        lspherePid.I = 0.0f;
        lspherePid.D = 0.0f;
        zeroThresh = 0.0f;
    }

    void LocospherePlayerSystem::shutdown(entt::registry&) {
    }
}
