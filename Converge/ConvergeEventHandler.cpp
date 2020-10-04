#include "ConvergeEventHandler.hpp"
#include <openvr.h>
#include <Log.hpp>
#include <Render.hpp>
#include "Transform.hpp"
#include <OpenVRInterface.hpp>
#include <Physics.hpp>
#include <Console.hpp>
#include <imgui.h>
#include <MatUtil.hpp>

namespace converge {
    void loadControllerRenderModel(const char* name, entt::entity ent, entt::registry& reg, worlds::VKRenderer* renderer) {
        if (!reg.valid(ent) || reg.has<worlds::ProceduralObject>(ent) || name == nullptr) return;

        vr::RenderModel_t* model;
        auto err = vr::VRRenderModels()->LoadRenderModel_Async(name, &model);
        if (err != vr::VRRenderModelError_None)
            return;

        if (model == nullptr) {
            logErr(worlds::WELogCategoryEngine, "SteamVR render model was null");
            return;
        }

        if (model->unVertexCount == 0 || model->unTriangleCount == 0) {
            logErr(worlds::WELogCategoryEngine, "SteamVR render model was invalid");
            return;
        }

        worlds::ProceduralObject& procObj = reg.emplace<worlds::ProceduralObject>(ent);
        procObj.material = worlds::g_assetDB.addOrGetExisting("Materials/dev.json");

        procObj.vertices.reserve(model->unVertexCount);

        for (uint32_t i = 0; i < model->unVertexCount; i++) {
            vr::RenderModel_Vertex_t vrVert = model->rVertexData[i];
            worlds::Vertex vert;
            vert.position = glm::vec3(vrVert.vPosition.v[0], vrVert.vPosition.v[1], vrVert.vPosition.v[2]);
            vert.normal = glm::vec3(vrVert.vNormal.v[0], vrVert.vNormal.v[1], vrVert.vNormal.v[2]);
            vert.uv = glm::vec2(vrVert.rfTextureCoord[0], vrVert.rfTextureCoord[1]);
            procObj.vertices.push_back(vert);
        }

        procObj.indices.reserve(model->unTriangleCount * 3);
        for (int i = 0; i < model->unTriangleCount * 3; i++) {
            procObj.indices.push_back(model->rIndexData[i]);
        }

        procObj.indexType = vk::IndexType::eUint32;
        procObj.readyForUpload = true;
        procObj.dbgName = name;

        renderer->uploadProcObj(procObj);

        logMsg("Loaded SteamVR render model %s with %u vertices and %u triangles", name, model->unVertexCount, model->unTriangleCount);
    }

    void cmdToggleVsync(void* obj, const char* arg) {
        auto renderer = (worlds::VKRenderer*)obj;
        renderer->setVsync(!renderer->getVsync());
    }

    struct FilterEntity : public physx::PxQueryFilterCallback {
        uint32_t entA;
        uint32_t entB;

        physx::PxQueryHitType::Enum preFilter(const physx::PxFilterData& filterData, const physx::PxShape* shape, const physx::PxRigidActor* actor, physx::PxHitFlags& queryFlags) override {
            return physx::PxQueryHitType::eBLOCK;
        }


        physx::PxQueryHitType::Enum postFilter(const physx::PxFilterData& filterData, const physx::PxQueryHit& hit) override {
            if ((uint32_t)hit.actor->userData == entA || (uint32_t)hit.actor->userData == entB) {
                return physx::PxQueryHitType::eNONE;
            }
            return physx::PxQueryHitType::eBLOCK;
        }
    };

    EventHandler::EventHandler()
        : lHandEnt{ entt::null }
        , rHandEnt{ entt::null }
        , playerLocosphere{ entt::null }
        , lHandPid{}
        , rHandPid{} {

    }

    physx::PxMaterial* locosphereMat;
    physx::PxMaterial* fenderMat;
    physx::PxD6Joint* fenderJoint;
    physx::PxD6Joint* headJoint;

    class NullPhysXCallback : public physx::PxRaycastCallback {
    public:
        NullPhysXCallback() : physx::PxRaycastCallback{ nullptr, 0 } {

        }

        physx::PxAgain processTouches(const physx::PxRaycastHit* buffer, physx::PxU32 nbHits) override {
            return false;
        }
    };

    void EventHandler::init(entt::registry& registry, worlds::EngineInterfaces interfaces) {
        vrInterface = interfaces.vrInterface;
        renderer = interfaces.renderer;
        camera = interfaces.mainCamera;
        inputManager = interfaces.inputManager;

        worlds::g_console->registerCommand(cmdToggleVsync, "r_toggleVsync", "Toggles Vsync.", renderer);
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

    void EventHandler::preSimUpdate(entt::registry& registry, float deltaTime) {
        if (!vrInterface) {
            jumpThisFrame = jumpThisFrame || inputManager->keyPressed(SDL_SCANCODE_SPACE);
        } else {
            jumpThisFrame = jumpThisFrame || vrInterface->getJumpInput();
        }
    }

    glm::vec3 lastDesiredVel;

    float locosphereAngVels[128];
    float locosphereLinVels[128];
    float lHandTorqueMag[128];
    float rHandTorqueMag[128];
    float lHandForceMag[128];
    float rHandForceMag[128];
    int physDbgIdx = 0;

    glm::vec3 offset;

    void EventHandler::update(entt::registry& registry, float deltaTime, float interpAlpha) {
        if (!vrInterface) {
            static float lookX = 0.0f, lookY = 0.0f;

            lookX += (float)(inputManager->getMouseDelta().x) * 0.005f;
            lookY += (float)(inputManager->getMouseDelta().y) * 0.005f;

            lookY = glm::clamp(lookY, -glm::half_pi<float>() + 0.001f, glm::half_pi<float>() - 0.001f);

            camera->rotation = glm::angleAxis(-lookX, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::angleAxis(lookY, glm::vec3(1.0f, 0.0f, 0.0f));
        }

        camera->position = glm::mix(lastCamPos, nextCamPos, interpAlpha);

        //ImGui::Text("Desired vel: %.3f, %.3f, %.3f", lastDesiredVel.x, lastDesiredVel.y, lastDesiredVel.z);
        if (ImGui::Begin("Locosphere")) {
            ImGui::PlotLines("Angular Speed", locosphereAngVels, 128, physDbgIdx, nullptr, 0.0f, 50.0f, ImVec2(500.0f, 100.0f));
            ImGui::PlotLines("Linear Speed", locosphereLinVels, 128, physDbgIdx, nullptr, 0.0f, 50.0f, ImVec2(500.0f, 100.0f));

            float maxAngVel = 0.0f;
            float maxLinVel = 0.0f;
            for (int i = 0; i < 128; i++) {
                maxLinVel = std::max(locosphereLinVels[i], maxLinVel);
                maxAngVel = std::max(locosphereAngVels[i], maxAngVel);
            }
            ImGui::Text("Maximum angular speed: %.3f", maxAngVel);
            ImGui::Text("Maximum linear speed: %.3f", maxLinVel);
        }
        ImGui::End();

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

            if (registry.has<worlds::ProceduralObject>(lHandEnt) && registry.has<worlds::ProceduralObject>(rHandEnt)) {
                auto& procObjL = registry.get<worlds::ProceduralObject>(lHandEnt);
                auto& procObjR = registry.get<worlds::ProceduralObject>(rHandEnt);
                procObjL.visible = !vr::VRSystem()->ShouldApplicationPause();
                procObjR.visible = !vr::VRSystem()->ShouldApplicationPause();
            }

            if (lHandRMName != nullptr && rHandRMName != nullptr) {
                loadControllerRenderModel(lHandRMName, lHandEnt, registry, renderer);
                loadControllerRenderModel(rHandRMName, rHandEnt, registry, renderer);
            }

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
    }

    float WrapAngle(float inputAngle) {
        //The inner % 360 restricts everything to +/- 360
        //+360 moves negative values to the positive range, and positive ones to > 360
        //the final % 360 caps everything to 0...360
        return fmodf((fmodf(inputAngle, 360.0f) + 360.0f), 360.0f);
    }

    float AngleToErr(float angle) {
        angle = WrapAngle(angle);
        if (angle > 180.0f && angle < 360.0f) {
            angle = 360.0f - angle;

            angle *= -1.0f;
        }

        return angle;
    }

    glm::quat fixupQuat(glm::quat q) {
        return q * glm::sign(glm::dot(q, glm::quat_identity<float, glm::packed_highp>()));
    }

    void EventHandler::simulate(entt::registry& registry, float simStep) {
        auto& locosphereTransform = registry.get<Transform>(playerLocosphere);

        const float maxSpeed = 50.0f;

        auto& wActor = registry.get<worlds::DynamicPhysicsActor>(playerLocosphere);
        auto* locosphereActor = (physx::PxRigidDynamic*)wActor.actor;

        FilterEntity filterEnt;
        filterEnt.entA = (uint32_t)playerLocosphere;
        filterEnt.entB = (uint32_t)playerFender;

        glm::vec3 desiredVel(0.0f);

        fenderJoint->setLocalPose(physx::PxJointActorIndex::eACTOR0, physx::PxTransform{ physx::PxVec3{0.0f, -0.8f,0.0f}, worlds::glm2px(glm::inverse(locosphereTransform.rotation)) });
        fenderJoint->setLocalPose(physx::PxJointActorIndex::eACTOR1, physx::PxTransform{ physx::PxVec3{0.0f, 0.0f,0.0f}, worlds::glm2px(glm::inverse(locosphereTransform.rotation)) });

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
        glm::quat rot = worlds::getMatrixRotation(camMat);

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

        static float locosphereAcceleration = 0.666f;
        //static float locosphereAcceleration = 0.666f;
        auto currVel = worlds::px2glm(locosphereActor->getAngularVelocity());
        glm::vec3 appliedTorque = (desiredVel - currVel);
        locosphereActor->addTorque(worlds::glm2px(appliedTorque * locosphereAcceleration * (1.0f / simStep)), physx::PxForceMode::eACCELERATION);

        NullPhysXCallback nullCallback{};
        bool grounded = worlds::g_scene->raycast(worlds::glm2px(locosphereTransform.position - glm::vec3(0.0f, 0.26f, 0.0f)), physx::PxVec3{ 0.0f, -1.0f, 0.0f }, 0.25f, nullCallback, physx::PxHitFlag::eDEFAULT, physx::PxQueryFilterData{ physx::PxQueryFlag::ePOSTFILTER | physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::eSTATIC }, &filterEnt);

        glm::vec3 currLinVel = worlds::px2glm(locosphereActor->getLinearVelocity());

        locosphereAngVels[physDbgIdx] = glm::length(currVel);
        locosphereLinVels[physDbgIdx] = glm::length(currLinVel);

        if (!grounded) {
            glm::vec3 airVel = camMat * glm::vec4(inputVel, 0.0f);
            if (vrInterface)
                airVel = -glm::vec3(airVel.z, 0.0f, airVel.x);
            airVel = airVel * glm::angleAxis(glm::half_pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));
            airVel *= 25.0f;
            glm::vec3 addedVel = (-airVel - currLinVel);
            addedVel.y = 0.0f;
            locosphereActor->addForce(worlds::glm2px(addedVel) * 0.25f, physx::PxForceMode::eACCELERATION);
        }

        if (jumpThisFrame) {
            if (grounded)
                locosphereActor->addForce(physx::PxVec3{ 0.0f, 7.0f, 0.0f }, physx::PxForceMode::eVELOCITY_CHANGE);
            jumpThisFrame = false;
        }

        if (grounded && glm::length2(desiredVel) < 0.1f && locosphereActor->getLinearVelocity().magnitudeSquared() < 0.1f) {
            locosphereActor->setMaxLinearVelocity(0.0f);
        } else {
            locosphereActor->setMaxLinearVelocity(PX_MAX_F32);
        }

        lastCamPos = nextCamPos;
        nextCamPos = locosphereTransform.position + glm::vec3(0.0f, -0.125f, 0.0f);

        if (!vrInterface) {
            // Make all non-VR users 1.75m tall
            nextCamPos += glm::vec3(0.0f, 1.75f - 0.25f, 0.0f);
        }

        if (vrInterface) {
            static glm::vec3 lastHeadPos{ 0.0f };
            //static float off = 0.0f;
            glm::vec3 headPos = worlds::getMatrixTranslation(vrInterface->getHeadTransform());
            glm::vec3 locosphereOffset = lastHeadPos - headPos;
            lastHeadPos = headPos;
            locosphereOffset.y = 0.0f;

            // magic 0.375 offset to account for radius of fender + head
            headJoint->setLocalPose(physx::PxJointActorIndex::eACTOR0, physx::PxTransform{ physx::PxVec3{0.0f, -headPos.y + 0.375f, 0.0f}, physx::PxQuat{physx::PxIdentity} });
            //ImGui::Text("VR Head Pos: %.2f, %.2f, %.2f", headPos.x, headPos.y, headPos.z);
            //ImGui::DragFloat("offs", &off);
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

                //if (body->getAngularVelocity().magnitudeSquared() > 10.0f)
                    //body->setAngularVelocity(physx::PxVec3{ 0.0f });

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

            if (!vrInterface->getActionHeld(throwHandAction)) {
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

    void EventHandler::onSceneStart(entt::registry& registry) {
        if (vrInterface) {
            auto lIdx = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::ETrackedControllerRole::TrackedControllerRole_LeftHand);
            auto rIdx = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::ETrackedControllerRole::TrackedControllerRole_RightHand);

            if (vr::VRSystem()->IsTrackedDeviceConnected(lIdx) && vr::VRSystem()->IsTrackedDeviceConnected(rIdx)) {
                size_t lSize = vr::VRSystem()->GetStringTrackedDeviceProperty(lIdx, vr::Prop_RenderModelName_String, nullptr, 0);
                size_t rSize = vr::VRSystem()->GetStringTrackedDeviceProperty(rIdx, vr::Prop_RenderModelName_String, nullptr, 0);

                lHandRMName = (char*)std::malloc(lSize);
                rHandRMName = (char*)std::malloc(rSize);

                vr::VRSystem()->GetStringTrackedDeviceProperty(lIdx, vr::Prop_RenderModelName_String, lHandRMName, (uint32_t)lSize);
                vr::VRSystem()->GetStringTrackedDeviceProperty(rIdx, vr::Prop_RenderModelName_String, rHandRMName, (uint32_t)rSize);
            }

            throwHandAction = vrInterface->getActionHandle("/actions/main/in/ThrowHand");

            lHandEnt = registry.create();
            registry.emplace<Transform>(lHandEnt);

            rHandEnt = registry.create();
            registry.emplace<Transform>(rHandEnt);

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

        // Locosphere
        playerLocosphere = registry.create();
        registry.emplace<Transform>(playerLocosphere).position = glm::vec3(0.0f, 2.0f, 0.0f);

        auto actor = worlds::g_physics->createRigidDynamic(physx::PxTransform{ physx::PxVec3{0.0f, 2.0f, 0.0f}, physx::PxQuat{physx::PxIdentity} });
        auto& wActor = registry.emplace<worlds::DynamicPhysicsActor>(playerLocosphere, actor);


        worlds::g_scene->addActor(*actor);

        locosphereMat = worlds::g_physics->createMaterial(0.5f, 15.0f, 0.0f);
        wActor.physicsShapes.push_back(worlds::PhysicsShape::sphereShape(0.25f, locosphereMat));

        worlds::updatePhysicsShapes(wActor);
        physx::PxRigidBodyExt::setMassAndUpdateInertia(*actor, 200.0f);

        playerFender = registry.create();
        registry.emplace<Transform>(playerFender);

        auto fenderActor = worlds::g_physics->createRigidDynamic(physx::PxTransform{ physx::PxVec3{0.0f, 0.5f, 0.0f}, physx::PxQuat{physx::PxIdentity} });
        auto& fenderWActor = registry.emplace<worlds::DynamicPhysicsActor>(playerFender, fenderActor);

        worlds::g_scene->addActor(*fenderActor);

        fenderMat = worlds::g_physics->createMaterial(0.0f, 0.0f, 0.0f);
        auto fenderShape = worlds::PhysicsShape::capsuleShape(0.05f, 0.5f, fenderMat);
        fenderShape.rot = glm::quat(glm::vec3(0.0f, 0.0f, glm::half_pi<float>()));
        fenderWActor.physicsShapes.push_back(fenderShape);

        worlds::updatePhysicsShapes(fenderWActor);
        physx::PxRigidBodyExt::setMassAndUpdateInertia(*fenderActor, 20.0f);

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

        playerHead = registry.create();

        auto headActor = worlds::g_physics->createRigidDynamic(physx::PxTransform{ physx::PxIdentity });
        auto& headWActor = registry.emplace<worlds::DynamicPhysicsActor>(playerHead, headActor);

        worlds::g_scene->addActor(*headActor);

        headWActor.physicsShapes.push_back(worlds::PhysicsShape::sphereShape(0.05f));

        headJoint = physx::PxD6JointCreate(*worlds::g_physics, headActor, physx::PxTransform{ physx::PxVec3{0.0f, -0.65f, 0.0f}, physx::PxQuat{physx::PxIdentity} }, fenderActor, physx::PxTransform{ physx::PxIdentity });

        physx::PxRigidBodyExt::setMassAndUpdateInertia(*headActor, 5.0f);

        headActor->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_X, true);
        headActor->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y, true);
        headActor->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z, true);

        headJoint->setMotion(physx::PxD6Axis::eX, physx::PxD6Motion::eLOCKED);
        headJoint->setMotion(physx::PxD6Axis::eY, physx::PxD6Motion::eLOCKED);
        headJoint->setMotion(physx::PxD6Axis::eZ, physx::PxD6Motion::eLOCKED);

        headJoint->setMotion(physx::PxD6Axis::eSWING1, physx::PxD6Motion::eFREE);
        headJoint->setMotion(physx::PxD6Axis::eSWING2, physx::PxD6Motion::eFREE);
        headJoint->setMotion(physx::PxD6Axis::eTWIST, physx::PxD6Motion::eFREE);

        actor->setSolverIterationCounts(15, 15);
        actor->setRigidBodyFlag(physx::PxRigidBodyFlag::eENABLE_SPECULATIVE_CCD, true);
        actor->setRigidBodyFlag(physx::PxRigidBodyFlag::eRETAIN_ACCELERATIONS, false);

        fenderJoint->setConstraintFlag(physx::PxConstraintFlag::eCOLLISION_ENABLED, false);
        fenderJoint->setConstraintFlag(physx::PxConstraintFlag::ePROJECTION, true);
        fenderJoint->setProjectionLinearTolerance(0.005f);

        fenderActor->setSolverIterationCounts(15, 15);
        fenderActor->setRigidBodyFlag(physx::PxRigidBodyFlag::eENABLE_SPECULATIVE_CCD, true);

        headActor->setSolverIterationCounts(15, 15);
        headJoint->setConstraintFlag(physx::PxConstraintFlag::ePROJECTION, true);
        headJoint->setProjectionLinearTolerance(0.005f);
        //headActor->setActorFlag(physx::PxActorFlag::eVISUALIZATION, false);

        worlds::updatePhysicsShapes(headWActor);

        //worlds::g_console->executeCommandStr("exec dbgscripts/shapes");

        //actor->setMaxDepenetrationVelocity(0.01f);
    }

    void EventHandler::shutdown(entt::registry& registry) {
        if (registry.valid(lHandEnt)) {
            registry.destroy(lHandEnt);
            registry.destroy(rHandEnt);
        }
    }
}
