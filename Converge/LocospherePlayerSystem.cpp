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
    physx::PxD6Joint* fenderJoint;

    worlds::ConVar showLocosphereDebug{ "cnvrg_showLocosphereDebug", "0", "Shows the locosphere debug menu." };

    glm::vec3 lastDesiredVel;

    float locosphereAngVels[128];
    float locosphereLinVels[128];
    float locosphereLinVelsXZ[128];
    float lHandTorqueMag[128];
    float rHandTorqueMag[128];
    float lHandForceMag[128];
    float rHandForceMag[128];
    int physDbgIdx = 0;
    bool grappling = false;
    glm::vec3 grappleTarget;
    physx::PxRigidDynamic* grappleBody;

    glm::vec3 offset;

    worlds::ConVar overallAmount{ "cnvrg_headBobAmount", "0.5" };
    worlds::ConVar headBobDbg{ "cnvrg_headBobDebug", "0" };
    worlds::ConVar speedometer{ "cnvrg_speedometer", "0" };
    worlds::ConVar mouseSensitivity { "cnvrg_mouseSensitivity", "1.0" };

    LocospherePlayerSystem::LocospherePlayerSystem(worlds::EngineInterfaces interfaces, entt::registry& registry)
        : vrInterface{ interfaces.vrInterface }
        , inputManager{ interfaces.inputManager }
        , registry{ registry }
        , camera{ interfaces.mainCamera }
        , lHandEnt{ entt::null }
        , rHandEnt{ entt::null }
        , playerLocosphere{ entt::null }
        , lHandPid{}
        , rHandPid{}
        , lookX{ 0.0f }
        , lookY{ 0.0f } {

        if (vrInterface) { 
            worlds::g_console->registerCommand([&](void*, const char*) {
                auto& wActor = registry.get<worlds::DynamicPhysicsActor>(lHandEnt);
                auto* body = static_cast<physx::PxRigidBody*>(wActor.actor);
                body->setLinearVelocity(physx::PxVec3{ 0.0f });

                auto& lTf = registry.get<Transform>(lHandEnt);
                auto lPose = body->getGlobalPose();
                lPose.p = worlds::glm2px(lHandWPos);
                lTf.position = lHandWPos;
                body->setGlobalPose(lPose);

                auto& wActorR = registry.get<worlds::DynamicPhysicsActor>(rHandEnt);
                auto* rBody = static_cast<physx::PxRigidBody*>(wActorR.actor);
                rBody->setLinearVelocity(physx::PxVec3{ 0.0f });

                auto& rTf = registry.get<Transform>(rHandEnt);
                auto rPose = rBody->getGlobalPose();
                rPose.p = worlds::glm2px(rHandWPos);
                rTf.position = rHandWPos;
                rBody->setGlobalPose(rPose);

                lHandPid.reset();
                rHandPid.reset();
            }, "cnvrg_resetHands", "Resets hand PID controllers.", nullptr);
        }
    }

    void LocospherePlayerSystem::preSimUpdate(entt::registry&, float) {
        if (!vrInterface) {
            jumpThisFrame = jumpThisFrame || inputManager->keyPressed(SDL_SCANCODE_SPACE);
        } else {
            jumpThisFrame = jumpThisFrame || vrInterface->getJumpInput();
        }
    }

    glm::vec3 LocospherePlayerSystem::calcHeadbobPosition(glm::vec3 desiredVel, glm::vec3 camPos, float deltaTime) {
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

    void LocospherePlayerSystem::update(entt::registry&, float deltaTime, float interpAlpha) {
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
                desiredVel.x += 1.0f;
            }

            if (inputManager->keyHeld(SDL_SCANCODE_S)) {
                desiredVel.x -= 1.0f;
            }

            if (inputManager->keyHeld(SDL_SCANCODE_A)) {
                desiredVel.z -= 1.0f;
            }

            if (inputManager->keyHeld(SDL_SCANCODE_D)) {
                desiredVel.z += 1.0f;
            }
        } 

        if (!vrInterface)
            camera->position = calcHeadbobPosition(desiredVel, camera->position, deltaTime);

        if (showLocosphereDebug.getInt()) {
            bool s = true;
            if (ImGui::Begin("Locosphere", &s)) {
                ImGui::Text("Grounded: %i", grounded);
                ImGui::PlotLines("Angular Speed", locosphereAngVels, 128, physDbgIdx, nullptr, 0.0f, 50.0f, ImVec2(500.0f, 100.0f));
                ImGui::PlotLines("Linear Speed", locosphereLinVels, 128, physDbgIdx, nullptr, 0.0f, 10.0f, ImVec2(500.0f, 100.0f));
                ImGui::PlotLines("Linear XZ Speed", locosphereLinVelsXZ, 128, physDbgIdx, nullptr, 0.0f, 10.0f, ImVec2(500.0f, 100.0f));

                float maxAngVel = 0.0f;
                float maxLinVel = 0.0f;
                float maxLinVelXZ = 0.0f;
                for (int i = 0; i < 128; i++) {
                    maxLinVel = std::max(locosphereLinVels[i], maxLinVel);
                    maxLinVelXZ = std::max(locosphereLinVelsXZ[i], maxLinVelXZ);
                    maxAngVel = std::max(locosphereAngVels[i], maxAngVel);
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

                ImGui::Text("Current angular speed: %.3f", locosphereAngVels[adjIdx]);
                ImGui::Text("Current linear speed: %.3f", locosphereLinVels[adjIdx]);
                ImGui::Text("Current linear speed (excluding Y): %.3f", locosphereLinVelsXZ[adjIdx]);

                ImGui::DragFloat("P", &lspherePid.P);
                ImGui::DragFloat("I", &lspherePid.I);
                ImGui::DragFloat("D", &lspherePid.D);
                ImGui::DragFloat("Zero Thresh", &zeroThresh, 0.01f);
            }
            ImGui::End();

            if (!s)
                showLocosphereDebug.setValue("0");
        }

        if (vrInterface) {
            if (ImGui::Begin("Hands")) {
                ImGui::Text("Positional PID");
                ImGui::DragFloat("P", &lHandPid.P);
                ImGui::DragFloat("I", &lHandPid.I);
                ImGui::DragFloat("D", &lHandPid.D);
                ImGui::DragFloat("D2", &lHandPid.D2);

                rHandPid.P = lHandPid.P;
                rHandPid.I = lHandPid.I;
                rHandPid.D = lHandPid.D;
                rHandPid.D2 = lHandPid.D2;

                ImGui::Separator();

                ImGui::Text("Rotational PID");
                ImGui::DragFloat("P##R", &lHandRotPid.P);
                ImGui::DragFloat("I##R", &lHandRotPid.I);
                ImGui::DragFloat("D##R", &lHandRotPid.D);
                ImGui::DragFloat("D2##R", &lHandRotPid.D2);

                rHandRotPid.P = lHandRotPid.P;
                rHandRotPid.I = lHandRotPid.I;
                rHandRotPid.D = lHandRotPid.D;
                rHandRotPid.D2 = lHandRotPid.D2;

                ImGui::Separator();
                ImGui::DragFloat3("Offset", &offset.x);

                ImGui::PlotLines("L Torque", lHandTorqueMag, 128, physDbgIdx, nullptr, 0.0f, 50.0f, ImVec2(500.0f, 100.0f));
                ImGui::PlotLines("L Force", lHandForceMag, 128, physDbgIdx, nullptr, 0.0f, 50.0f, ImVec2(500.0f, 100.0f));
                ImGui::PlotLines("R Torque", rHandTorqueMag, 128, physDbgIdx, nullptr, 0.0f, 50.0f, ImVec2(500.0f, 100.0f));
                ImGui::PlotLines("R Force", rHandForceMag, 128, physDbgIdx, nullptr, 0.0f, 50.0f, ImVec2(500.0f, 100.0f));
            }
            ImGui::End();

            auto lIdx = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::ETrackedControllerRole::TrackedControllerRole_LeftHand);
            auto rIdx = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::ETrackedControllerRole::TrackedControllerRole_RightHand);

            if (vr::VRSystem()->IsTrackedDeviceConnected(lIdx)) {
                Transform t;

                if (((worlds::OpenVRInterface*)vrInterface)->getHandTransform(vr::ETrackedControllerRole::TrackedControllerRole_LeftHand, t)) {
                    ImGui::Text("Left Controller: %.2f, %.2f, %.2f", t.position.x, t.position.y, t.position.z);
                    t.position += t.rotation * offset;
                    t.fromMatrix(glm::inverse(camera->getViewMatrix()) * t.getMatrix());
                    ImGui::Text("Left Controller World: %.2f, %.2f, %.2f", t.position.x, t.position.y, t.position.z);
                    lHandWPos = t.position;
                    lHandWRot = t.rotation;
                }
            }

            if (vr::VRSystem()->IsTrackedDeviceConnected(rIdx)) {
                Transform t;

                if (((worlds::OpenVRInterface*)vrInterface)->getHandTransform(vr::ETrackedControllerRole::TrackedControllerRole_RightHand, t)) {
                    ImGui::Text("Right Controller: %.2f, %.2f, %.2f", t.position.x, t.position.y, t.position.z);
                    t.position += t.rotation * offset;
                    t.fromMatrix(glm::inverse(camera->getViewMatrix()) * t.getMatrix());
                    ImGui::Text("Right Controller World: %.2f, %.2f, %.2f", t.position.x, t.position.y, t.position.z);
                    rHandWPos = t.position + (offset * t.rotation);
                    rHandWRot = t.rotation;
                }
            }
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

            sprintf(buf, "total vel: %.3fms^-1", locosphereLinVels[adjIdx]);
            sprintf(buf2, "xz vel: %.3fms^-1", locosphereLinVelsXZ[adjIdx]);

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

        if (!vrInterface) {
            if (inputManager->mouseButtonPressed(worlds::MouseButton::Right)) {
                NullPhysXCallback nullCallback{};
                physx::PxRaycastBuffer hitBuf;
                bool hit = worlds::g_scene->raycast(worlds::glm2px(camera->position), worlds::glm2px(camera->rotation * glm::vec3{ 0.0f, 0.0f, 1.0f }), FLT_MAX, hitBuf);

                if (hit && hitBuf.hasBlock) {
                    grappling = true;
                    auto& touch = hitBuf.block;

                    grappleBody = touch.actor->is<physx::PxRigidDynamic>();
                    grappleTarget = worlds::px2glm(touch.position);

                    if (grappleBody) {
                        grappleTarget = worlds::px2glm(grappleBody->getGlobalPose().transformInv(worlds::glm2px(grappleTarget)));
                    }
                }
            }

            if (grappling && !inputManager->mouseButtonHeld(worlds::MouseButton::Right)) {
                grappling = false;
            }
        } else {
            if (vrInterface->getActionPressed(grappleHookAction)) {
                NullPhysXCallback nullCallback{};
                physx::PxRaycastBuffer hitBuf;

                Transform& lHandTranform = registry.get<Transform>(lHandEnt);
                glm::vec3 start = lHandTranform.position;
                glm::vec3 dir = lHandTranform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
                start += dir * 0.25f;
                FilterEntity f;
                f.entA = lHandEnt;
                f.entB = rHandEnt;
                bool hit = worlds::g_scene->raycast(worlds::glm2px(start), worlds::glm2px(dir), FLT_MAX, hitBuf, physx::PxHitFlag::eDEFAULT, physx::PxQueryFilterData(), &f);

                if (hit && hitBuf.hasBlock) {
                    grappling = true;
                    auto& touch = hitBuf.block;

                    grappleBody = touch.actor->is<physx::PxRigidDynamic>();
                    grappleTarget = worlds::px2glm(touch.position);

                    logMsg("we grapplin' to %.3f, %.3f, %.3f", grappleTarget.x, grappleTarget.y, grappleTarget.z);

                    if (grappleBody) {
                        logMsg("we grapplin' to something MOVING :O");
                        entt::entity grappleEnt = getActorEntity(grappleBody);
                        if (registry.valid(grappleEnt) && registry.has<worlds::NameComponent>(grappleEnt)) {
                            logMsg("(its name is %s)", registry.get<worlds::NameComponent>(grappleEnt).name.c_str());
                        }
                        grappleTarget = worlds::px2glm(grappleBody->getGlobalPose().transformInv(worlds::glm2px(grappleTarget)));
                    }
                }
            }

            if (grappling && vrInterface->getActionReleased(grappleHookAction)) {
                logMsg("we no longer grapplin' :(");
                grappling = false;
            }
        }

        if (grappling) {
            glm::vec3 actualGrappleTarget = grappleTarget;

            if (grappleBody) {
                actualGrappleTarget = worlds::px2glm(grappleBody->getGlobalPose().transform(worlds::glm2px(grappleTarget)));
            }

            ImVec2 vSize = ImGui::GetMainViewport()->Size;
            // Convert selected transform position from world space to screen space
            glm::vec4 ndcObjPosPreDivide = camera->getProjectionMatrix(vSize.x / vSize.y) * camera->getViewMatrix() * glm::vec4(actualGrappleTarget, 1.0f);

            // NDC -> screen space
            glm::vec2 ndcObjectPosition(ndcObjPosPreDivide);
            ndcObjectPosition /= ndcObjPosPreDivide.w;
            ndcObjectPosition *= 0.5f;
            ndcObjectPosition += 0.5f;
            ndcObjectPosition *= glm::vec2(vSize.x, vSize.y);
            // Not sure why flipping Y is necessary?
            ndcObjectPosition.y = vSize.y - ndcObjectPosition.y;

            if ((ndcObjPosPreDivide.z / ndcObjPosPreDivide.w) > 0.0f)
                ImGui::GetBackgroundDrawList()->AddCircle(ImVec2(ndcObjectPosition.x, ndcObjectPosition.y), 500.0f / ndcObjPosPreDivide.w, ImColor(1.0f, 1.0f, 1.0f));
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
        auto& locosphereTransform = registry.get<Transform>(playerLocosphere);

        const float maxSpeed = 15.0f;

        auto& wActor = registry.get<worlds::DynamicPhysicsActor>(playerLocosphere);
        auto* locosphereActor = (physx::PxRigidDynamic*)wActor.actor;

        FilterEntity filterEnt;
        filterEnt.entA = playerLocosphere;
        filterEnt.entB = playerFender;

        glm::vec3 desiredVel(0.0f);

        if (!vrInterface) {
            if (inputManager->keyHeld(SDL_SCANCODE_W)) {
                desiredVel.x += 1.0f;
            }

            if (inputManager->keyHeld(SDL_SCANCODE_S)) {
                desiredVel.x -= 1.0f;
            }

            if (inputManager->keyHeld(SDL_SCANCODE_A)) {
                desiredVel.z -= 1.0f;
            }

            if (inputManager->keyHeld(SDL_SCANCODE_D)) {
                desiredVel.z += 1.0f;
            }
        } else {
            vrInterface->updateInput();

            auto locIn = vrInterface->getLocomotionInput();
            desiredVel = glm::vec3(locIn.x, 0.0f, locIn.y);
        }

        glm::vec3 inputVel = desiredVel;

        if (glm::length2(desiredVel) > 0.0f)
            desiredVel = glm::normalize(desiredVel);

        glm::mat4 camMat;
        if (vrInterface) {
            camMat = vrInterface->getHeadTransform();

            // Account for OpenVR weirdness
            desiredVel = -glm::vec3(desiredVel.z, 0.0f, desiredVel.x);
        } else {
            camMat = glm::inverse(camera->getViewMatrix());
        }

        desiredVel = camMat * glm::vec4(desiredVel, 0.0f);
        desiredVel.y = 0.0f;

        if (glm::length2(desiredVel) > 0.0f)
            desiredVel = glm::normalize(-desiredVel);

        desiredVel *= maxSpeed;

        if (vrInterface && vrInterface->getSprintInput()) {
            desiredVel *= 2.0f;
        }

        if (inputManager->keyHeld(SDL_SCANCODE_LSHIFT)) {
            desiredVel *= 2.0f;
        }

        lastDesiredVel = desiredVel;

        auto currVel = worlds::px2glm(locosphereActor->getAngularVelocity());
        glm::vec3 torque = lspherePid.getOutput(desiredVel - currVel, simStep);
        locosphereActor->addTorque(worlds::glm2px(torque), physx::PxForceMode::eACCELERATION);

        if (grappling) {
            glm::vec3 actualGrappleTarget = grappleTarget;

            if (grappleBody) {
                actualGrappleTarget = worlds::px2glm(grappleBody->getGlobalPose().transform(worlds::glm2px(grappleTarget)));
            }

            physx::PxVec3 force = worlds::glm2px(glm::normalize(actualGrappleTarget - locosphereTransform.position)) * 2500.0f;

            if (grappleBody) {
                force *= 0.5f;

                // Clamp impulse
                force /= grappleBody->getMass();
                force = clampMagnitude(force, 30.0f);
                force *= grappleBody->getMass();
            }

            locosphereActor->addForce(force);

            if (grappleBody) {
                auto comWorldSpace = worlds::px2glm((grappleBody->getGlobalPose() * grappleBody->getCMassLocalPose()).p);
                auto torque = glm::cross(actualGrappleTarget - comWorldSpace, worlds::px2glm(-force));
                grappleBody->addTorque(worlds::glm2px(torque));
                grappleBody->addForce(-force);
            }
        }

        if (locosphereActor->getLinearVelocity().magnitudeSquared() < zeroThresh * zeroThresh) {
            locosphereActor->setLinearVelocity(physx::PxVec3(0.0f, 0.0f, 0.0f));
        }

        NullPhysXCallback nullCallback{};
        grounded = worlds::g_scene->raycast(worlds::glm2px(locosphereTransform.position - glm::vec3(0.0f, LOCOSPHERE_RADIUS - 0.01f, 0.0f)), physx::PxVec3{ 0.0f, -1.0f, 0.0f }, LOCOSPHERE_RADIUS, nullCallback, physx::PxHitFlag::eDEFAULT, physx::PxQueryFilterData{ physx::PxQueryFlag::ePOSTFILTER | physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::eSTATIC }, &filterEnt);

        glm::vec3 currLinVel = worlds::px2glm(locosphereActor->getLinearVelocity());

        locosphereAngVels[physDbgIdx] = glm::length(currVel);
        locosphereLinVels[physDbgIdx] = glm::length(currLinVel);
        locosphereLinVelsXZ[physDbgIdx] = glm::length(glm::vec3{ currLinVel.x, 0.0f, currLinVel.z });

        if (!grounded) {
            if (vrInterface) {
                inputVel = -glm::vec3(inputVel.z, 0.0f, inputVel.x);
            }

            glm::vec3 airVel = camMat * glm::vec4(inputVel, 0.0f);
            airVel = airVel * glm::angleAxis(glm::half_pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));
            airVel *= 25.0f;
            glm::vec3 addedVel = (-airVel - currLinVel);
            addedVel.y = 0.0f;
            locosphereActor->addForce(worlds::glm2px(addedVel) * 0.25f, physx::PxForceMode::eACCELERATION);
        }

        if (jumpThisFrame) {
            if (grounded)
                locosphereActor->addForce(physx::PxVec3{ 0.0f, 7.5f, 0.0f }, physx::PxForceMode::eVELOCITY_CHANGE);
            jumpThisFrame = false;
        }

        lastCamPos = nextCamPos;
        nextCamPos = locosphereTransform.position + glm::vec3(0.0f, -LOCOSPHERE_RADIUS, 0.0f);

        if (!vrInterface) {
            // Make all non-VR users 1.75m tall
            // Then subtract 5cm from that to account for eye offset
            nextCamPos += glm::vec3(0.0f, 1.7f, 0.0f);
        }

        if (vrInterface) {
            static glm::vec3 lastHeadPos{ 0.0f };
            //static float off = 0.0f;
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

            auto fenderActor = registry.get<worlds::DynamicPhysicsActor>(playerFender).actor;
            auto fenderWorldsTf = registry.get<Transform>(playerFender);
            physx::PxTransform fenderTf = fenderActor->getGlobalPose();
            fenderTf.p += worlds::glm2px(locosphereOffset);
            fenderWorldsTf.position += locosphereOffset;
            fenderActor->setGlobalPose(fenderTf);

            {
                auto& wActor = registry.get<worlds::DynamicPhysicsActor>(lHandEnt);
                auto* body = static_cast<physx::PxRigidBody*>(wActor.actor);
                auto& tf = registry.get<Transform>(lHandEnt);
                glm::vec3 err = lHandWPos - tf.position;
                glm::vec3 force = lHandPid.getOutput(err, simStep);
                body->addForce(worlds::glm2px(force));
                lHandForceMag[physDbgIdx] = glm::length(force);

                glm::quat filteredQ = glm::normalize(lHandWRot);

                filteredQ = fixupQuat(filteredQ);

                glm::quat quaternionDifference = filteredQ * glm::inverse(fixupQuat(tf.rotation));

                quaternionDifference = fixupQuat(quaternionDifference);

                float angle = glm::angle(quaternionDifference);
                glm::vec3 axis = glm::axis(quaternionDifference);
                angle = glm::degrees(angle);
                angle = AngleToErr(angle);
                angle = glm::radians(angle);

                glm::vec3 torque = lHandRotPid.getOutput(angle * axis, simStep);
                lHandTorqueMag[physDbgIdx] = glm::length(torque);

                if (!glm::any(glm::isnan(axis)) && !glm::any(glm::isinf(torque)))
                    body->addTorque(worlds::glm2px(torque));
            }

            {
                auto& wActor = registry.get<worlds::DynamicPhysicsActor>(rHandEnt);
                auto* body = static_cast<physx::PxRigidBody*>(wActor.actor);
                auto& tf = registry.get<Transform>(rHandEnt);
                glm::vec3 err = rHandWPos - tf.position;
                glm::vec3 force = rHandPid.getOutput(err, simStep);
                body->addForce(worlds::glm2px(force));
                rHandForceMag[physDbgIdx] = glm::length(force);

                glm::quat filteredQ = glm::normalize(rHandWRot);

                filteredQ = fixupQuat(filteredQ);

                glm::quat quaternionDifference = filteredQ * glm::inverse(fixupQuat(tf.rotation));

                quaternionDifference = fixupQuat(quaternionDifference);

                float angle = glm::angle(quaternionDifference);
                glm::vec3 axis = glm::axis(quaternionDifference);
                angle = glm::degrees(angle);
                angle = AngleToErr(angle);
                angle = glm::radians(angle);

                glm::vec3 torque = rHandRotPid.getOutput(angle * axis, simStep);
                rHandTorqueMag[physDbgIdx] = glm::length(force);

                if (!glm::any(glm::isnan(axis)) && !glm::any(glm::isinf(torque)))
                    body->addTorque(worlds::glm2px(torque));
            }
        }

        physDbgIdx++;
        if (physDbgIdx == 128)
            physDbgIdx = 0;
    }

    void LocospherePlayerSystem::onSceneStart(entt::registry& registry) {
        if (vrInterface) {
            grappleHookAction = vrInterface->getActionHandle("/actions/main/in/ThrowHand");

            auto matId = worlds::g_assetDB.addOrGetExisting("Materials/dev.json");

            lHandEnt = registry.create();
            registry.emplace<worlds::WorldObject>(lHandEnt, matId, worlds::g_assetDB.addOrGetExisting("pinhand.obj"));
            registry.emplace<Transform>(lHandEnt).scale = glm::vec3(0.25f);
            registry.emplace<worlds::NameComponent>(lHandEnt).name = "L. Handy";

            rHandEnt = registry.create();
            registry.emplace<worlds::WorldObject>(rHandEnt, matId, worlds::g_assetDB.addOrGetExisting("pinhand.obj"));
            registry.emplace<Transform>(rHandEnt).scale = glm::vec3(0.25f);
            registry.emplace<worlds::NameComponent>(rHandEnt).name = "R. Handy";

            auto lActor = worlds::g_physics->createRigidDynamic(physx::PxTransform{ physx::PxIdentity });
            // Using the reference returned by this doesn't work unfortunately.
            registry.emplace<worlds::DynamicPhysicsActor>(lHandEnt, lActor);

            auto rActor = worlds::g_physics->createRigidDynamic(physx::PxTransform{ physx::PxIdentity });
            auto& rwActor = registry.emplace<worlds::DynamicPhysicsActor>(rHandEnt, rActor);
            auto& lwActor = registry.get<worlds::DynamicPhysicsActor>(lHandEnt);

            rwActor.physicsShapes.emplace_back(worlds::PhysicsShape::sphereShape(0.1f));
            lwActor.physicsShapes.emplace_back(worlds::PhysicsShape::sphereShape(0.1f));

            worlds::updatePhysicsShapes(rwActor);
            worlds::updatePhysicsShapes(lwActor);

            worlds::g_scene->addActor(*rActor);
            worlds::g_scene->addActor(*lActor);

            physx::PxRigidBodyExt::setMassAndUpdateInertia(*rActor, 2.0f);
            physx::PxRigidBodyExt::setMassAndUpdateInertia(*lActor, 2.0f);

            lHandPid.P = 1370.0f;
            lHandPid.D = 100.0f;

            rHandPid.P = 1370.0f;
            rHandPid.D = 100.0f;

            lHandRotPid.P = 2.5f;
            lHandRotPid.D = 0.2f;

            rHandRotPid.P = 2.5f;
            rHandRotPid.D = 0.2f;
        }

        // Create physics rig
        lspherePid.P = 50.0f;
        lspherePid.I = 0.0f;
        lspherePid.D = 0.0f;
        zeroThresh = 0.0f;

        // Locosphere
        playerLocosphere = registry.create();
        registry.emplace<Transform>(playerLocosphere).position = glm::vec3(0.0f, 2.0f, 0.0f);

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
        playerFender = registry.create();
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

        fenderJoint = physx::PxD6JointCreate(*worlds::g_physics, fenderActor, physx::PxTransform{ physx::PxVec3{0.0f, -0.4f, 0.0f}, physx::PxQuat{ physx::PxIdentity } }, actor, physx::PxTransform{ physx::PxIdentity });

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
    }

    void LocospherePlayerSystem::shutdown(entt::registry& registry) {
        if (registry.valid(lHandEnt)) {
            registry.destroy(lHandEnt);
            registry.destroy(rHandEnt);
        }
    }
}
