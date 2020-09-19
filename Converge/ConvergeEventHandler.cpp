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

        for (int i = 0; i < model->unVertexCount; i++) {
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

    EventHandler::EventHandler()
        : lHandEnt(entt::null)
        , rHandEnt(entt::null)
        , playerLocosphere(entt::null) {

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

        if (vrInterface) {
            auto lIdx = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::ETrackedControllerRole::TrackedControllerRole_LeftHand);
            auto rIdx = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::ETrackedControllerRole::TrackedControllerRole_RightHand);

            if (vr::VRSystem()->IsTrackedDeviceConnected(lIdx) && vr::VRSystem()->IsTrackedDeviceConnected(rIdx)) {
                size_t lSize = vr::VRSystem()->GetStringTrackedDeviceProperty(lIdx, vr::Prop_RenderModelName_String, nullptr, 0);
                size_t rSize = vr::VRSystem()->GetStringTrackedDeviceProperty(rIdx, vr::Prop_RenderModelName_String, nullptr, 0);

                lHandRMName = (char*)std::malloc(lSize);
                rHandRMName = (char*)std::malloc(rSize);

                vr::VRSystem()->GetStringTrackedDeviceProperty(lIdx, vr::Prop_RenderModelName_String, lHandRMName, lSize);
                vr::VRSystem()->GetStringTrackedDeviceProperty(rIdx, vr::Prop_RenderModelName_String, rHandRMName, rSize);
            }

            lHandEnt = registry.create();
            registry.emplace<Transform>(lHandEnt);
            rHandEnt = registry.create();
            registry.emplace<Transform>(rHandEnt);
        }

        // Create physics rig

        // Locosphere
        playerLocosphere = registry.create();
        registry.emplace<Transform>(playerLocosphere).position = glm::vec3(0.0f, 2.0f, 0.0f);

        auto actor = worlds::g_physics->createRigidDynamic(physx::PxTransform{ physx::PxVec3{0.0f, 2.0f, 0.0f}, physx::PxQuat{physx::PxIdentity} });
        auto& wActor = registry.emplace<worlds::DynamicPhysicsActor>(playerLocosphere, actor);


        worlds::g_scene->addActor(*actor);

        locosphereMat = worlds::g_physics->createMaterial(0.5f, 10.0f, 0.0f);
        wActor.physicsShapes.push_back(worlds::PhysicsShape::sphereShape(0.125f, locosphereMat));

        worlds::updatePhysicsShapes(wActor);
        physx::PxRigidBodyExt::setMassAndUpdateInertia(*actor, 20.0f);

        playerFender = registry.create();
        registry.emplace<Transform>(playerFender);

        auto fenderActor = worlds::g_physics->createRigidDynamic(physx::PxTransform{ physx::PxVec3{0.0f, 0.25f, 0.0f}, physx::PxQuat{physx::PxIdentity} });
        auto& fenderWActor = registry.emplace<worlds::DynamicPhysicsActor>(playerFender, fenderActor);

        worlds::g_scene->addActor(*fenderActor);

        fenderMat = worlds::g_physics->createMaterial(0.0f, 0.0f, 0.0f);
        fenderWActor.physicsShapes.push_back(worlds::PhysicsShape::boxShape(glm::vec3(0.15f, 0.35f, 0.15f), fenderMat));

        worlds::updatePhysicsShapes(fenderWActor);
        physx::PxRigidBodyExt::setMassAndUpdateInertia(*fenderActor, 1.0f);

        fenderJoint = physx::PxD6JointCreate(*worlds::g_physics, fenderActor, physx::PxTransform{ physx::PxVec3{0.0f, -0.25f, 0.0f}, physx::PxQuat{physx::PxIdentity} }, actor, physx::PxTransform{ physx::PxIdentity });

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

        headJoint = physx::PxD6JointCreate(*worlds::g_physics, headActor, physx::PxTransform{ physx::PxVec3{0.0f, -1.375f, 0.0f}, physx::PxQuat{physx::PxIdentity} }, fenderActor, physx::PxTransform{ physx::PxIdentity });
        
        physx::PxRigidBodyExt::setMassAndUpdateInertia(*headActor, 1.0f);

        headActor->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_X, true);
        headActor->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y, true);
        headActor->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z, true);

        headJoint->setMotion(physx::PxD6Axis::eX, physx::PxD6Motion::eLOCKED);
        headJoint->setMotion(physx::PxD6Axis::eY, physx::PxD6Motion::eLOCKED);
        headJoint->setMotion(physx::PxD6Axis::eZ, physx::PxD6Motion::eLOCKED);

        headJoint->setMotion(physx::PxD6Axis::eSWING1, physx::PxD6Motion::eFREE);
        headJoint->setMotion(physx::PxD6Axis::eSWING2, physx::PxD6Motion::eFREE);
        headJoint->setMotion(physx::PxD6Axis::eTWIST, physx::PxD6Motion::eFREE);

        actor->setSolverIterationCounts(50, 50);
        fenderActor->setSolverIterationCounts(50, 50);
        headActor->setSolverIterationCounts(50, 50);

        worlds::updatePhysicsShapes(headWActor);

        worlds::g_console->registerCommand(cmdToggleVsync, "r_toggleVsync", "Toggles Vsync.", renderer);
    }

    void EventHandler::preSimUpdate(entt::registry& registry, float deltaTime) {
        if (!vrInterface) {
            jumpThisFrame = jumpThisFrame || inputManager->keyPressed(SDL_SCANCODE_SPACE);
        } else {
            jumpThisFrame = jumpThisFrame || vrInterface->getJumpInput();
        }
    }

    glm::vec3 lastDesiredVel;

    void EventHandler::update(entt::registry& registry, float deltaTime, float interpAlpha) {
        if (!vrInterface) {
            static float lookX = 0.0f, lookY = 0.0f;

            lookX += (float)(inputManager->getMouseDelta().x) * 0.005f;
            lookY += (float)(inputManager->getMouseDelta().y) * 0.005f;

            lookY = glm::clamp(lookY, -glm::half_pi<float>() + 0.001f, glm::half_pi<float>() - 0.001f);

            camera->rotation = glm::angleAxis(-lookX, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::angleAxis(lookY, glm::vec3(1.0f, 0.0f, 0.0f));
        }

        camera->position = glm::mix(lastCamPos, nextCamPos, interpAlpha);


        ImGui::Text("Desired vel: %.3f, %.3f, %.3f", lastDesiredVel.x, lastDesiredVel.y, lastDesiredVel.z);

        if (vrInterface) {
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
                auto& transform = registry.get<Transform>(lHandEnt);
                transform.scale = glm::vec3(1.0f);

                if (((worlds::OpenVRInterface*)vrInterface)->getHandTransform(vr::ETrackedControllerRole::TrackedControllerRole_LeftHand, transform)) {
                    ImGui::Text("Left Controller: %.2f, %.2f, %.2f", transform.position.x, transform.position.y, transform.position.z);
                    transform.fromMatrix(glm::inverse(camera->getViewMatrix()) * transform.getMatrix());
                    ImGui::Text("Left Controller World: %.2f, %.2f, %.2f", transform.position.x, transform.position.y, transform.position.z);
                }
            }

            if (vr::VRSystem()->IsTrackedDeviceConnected(rIdx)) {
                auto& transform = registry.get<Transform>(rHandEnt);
                transform.scale = glm::vec3(1.0f);

                if (((worlds::OpenVRInterface*)vrInterface)->getHandTransform(vr::ETrackedControllerRole::TrackedControllerRole_RightHand, transform)) {
                    ImGui::Text("Right Controller: %.2f, %.2f, %.2f", transform.position.x, transform.position.y, transform.position.z);
                    transform.fromMatrix(glm::inverse(camera->getViewMatrix()) * transform.getMatrix());
                    ImGui::Text("Right Controller World: %.2f, %.2f, %.2f", transform.position.x, transform.position.y, transform.position.z);
                }
            }
        }
    }

    void EventHandler::simulate(entt::registry& registry, float simStep) {
        auto& locosphereTransform = registry.get<Transform>(playerLocosphere);

        const float maxSpeed = 30.0f;

        auto& wActor = registry.get<worlds::DynamicPhysicsActor>(playerLocosphere);
        auto* locosphereActor = (physx::PxRigidDynamic*)wActor.actor;

        glm::vec3 desiredVel(0.0f);

        fenderJoint->setLocalPose(physx::PxJointActorIndex::eACTOR1, physx::PxTransform{ physx::PxVec3{0.0f,0.0f,0.0f}, worlds::glm2px(glm::inverse(locosphereTransform.rotation)) });

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
        auto currVel = worlds::px2glm(locosphereActor->getAngularVelocity());
        glm::vec3 appliedTorque = (desiredVel - currVel);
        locosphereActor->addTorque(worlds::glm2px(appliedTorque * locosphereAcceleration * (1.0f / simStep)), physx::PxForceMode::eACCELERATION);

        NullPhysXCallback nullCallback{};
        bool grounded = worlds::g_scene->raycast(worlds::glm2px(locosphereTransform.position - glm::vec3(0.0f, 0.251f, 0.0f)), physx::PxVec3{ 0.0f, -1.0f, 0.0f }, 0.05f, nullCallback);

        if (jumpThisFrame && grounded) {
            locosphereActor->addForce(physx::PxVec3{ 0.0f, 7.0f, 0.0f }, physx::PxForceMode::eVELOCITY_CHANGE);
            jumpThisFrame = false;
        }

        lastCamPos = nextCamPos;
        nextCamPos = locosphereTransform.position + glm::vec3(0.0f, -0.125f, 0.0f);

        if (!vrInterface) {
            // Make all non-VR users 1.75m tall
            nextCamPos += glm::vec3(0.0f, 1.75f, 0.0f);
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
            locosphereptf.p += worlds::glm2px(locosphereOffset);
            locosphereTransform.position += locosphereOffset;
            locosphereActor->setGlobalPose(locosphereptf);
        }
    }

    void EventHandler::shutdown(entt::registry& registry) {
        if (registry.valid(lHandEnt)) {
            registry.destroy(lHandEnt);
            registry.destroy(rHandEnt);
        }
    }
}
