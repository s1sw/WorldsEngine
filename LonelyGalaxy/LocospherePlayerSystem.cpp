#include "LocospherePlayerSystem.hpp"
#include <Physics/Physics.hpp>
#include <ImGui/imgui.h>
#include <openvr.h>
#include <VR/OpenVRInterface.hpp>
#include <Core/NameComponent.hpp>
#include <Core/Engine.hpp>
#include <Core/Console.hpp>
#include <Input/Input.hpp>
#include <Render/Camera.hpp>
#include "MathsUtil.hpp"
#include <Util/CreateModelObject.hpp>
#include "DebugArrow.hpp"
#include <Physics/D6Joint.hpp>
#include <Audio/Audio.hpp>
#include <Libs/pcg_basic.h>

namespace lg {
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

    const float LOCOSPHERE_RADIUS = 0.15f;

    physx::PxMaterial* locosphereMat;
    physx::PxMaterial* fenderMat;
    physx::PxD6Joint* lHandJoint;
    physx::PxD6Joint* rHandJoint;

    worlds::ConVar showLocosphereDebug{ "lg_showLocosphereDebug", "0", "Shows the locosphere debug menu." };
    worlds::ConVar drawDbgArrows { "lg_locoDbgArrow", "0", "Draw a debug arrow pointing in the current input direction." };

    glm::vec3 lastDesiredVel;

    int physDbgIdx = 0;

    worlds::ConVar overallAmount{ "lg_headBobAmount", "0.5" };
    worlds::ConVar headBobDbg{ "lg_headBobDebug", "0" };
    worlds::ConVar speedometer{ "lg_speedometer", "0" };
    worlds::ConVar mouseSensitivity { "lg_mouseSensitivity", "1.0" };

    struct LocosphereDebugInfo {
        float angVels[128];
        float linVels[128];
        float linVelsXZ[128];
    };

    std::unordered_map<entt::entity, LocosphereDebugInfo> locosphereDebug;
    std::vector<worlds::AssetID> footstepSounds;
    worlds::AssetID jumpSound;
    worlds::AssetID landSound;
    std::vector<worlds::AssetID> doubleJumpSounds;
    worlds::AssetID wallJumpSound;
    pcg32_random_t rng;

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

        for (int i = 1; i <= 10; i++) {
            footstepSounds.push_back(worlds::AssetDB::pathToId("Audio/SFX/Footsteps/Concrete/step" + std::string(i < 10 ? "0" : "") + std::to_string(i) + ".ogg"));
        }

        jumpSound = worlds::AssetDB::pathToId("Audio/SFX/Player/jump.ogg");
        landSound = worlds::AssetDB::pathToId("Audio/SFX/Footsteps/Concrete/land.ogg");

        std::string baseDblJumpPath = "Audio/SFX/Player/double_jump";
        doubleJumpSounds.push_back(worlds::AssetDB::pathToId(baseDblJumpPath + ".ogg"));
        doubleJumpSounds.push_back(worlds::AssetDB::pathToId(baseDblJumpPath + "2.ogg"));
        doubleJumpSounds.push_back(worlds::AssetDB::pathToId(baseDblJumpPath + "3.ogg"));

        wallJumpSound = worlds::AssetDB::pathToId("Audio/SFX/Player/wall_jump.ogg");
        pcg32_srandom_r(&rng, 135u, 3151u);
    }

    void LocospherePlayerSystem::onPlayerConstruct(entt::registry&, entt::entity ent) {
        locosphereDebug.insert({ ent, LocosphereDebugInfo {} });
    }

    void LocospherePlayerSystem::onPlayerDestroy(entt::registry&, entt::entity ent) {
        locosphereDebug.erase(ent);
    }

    void LocospherePlayerSystem::updatePlayerSounds(entt::entity locosphereEnt,
            float deltaTime) {
        auto& lpc = registry.get<LocospherePlayerComponent>(locosphereEnt);
        static float timeSinceLastJump = 0.0f;
        static bool groundedLast = false;
        static bool dblJumpUsedLast = false;

        timeSinceLastJump += deltaTime;

        bool jump = vrInterface ? vrInterface->getJumpInput() : inputManager->keyPressed(SDL_SCANCODE_SPACE);

        if (jump && timeSinceLastJump > 0.2f) {
            auto& t = registry.get<Transform>(locosphereEnt);
            lpc.jump = true;
            timeSinceLastJump = 0.0f;
            if (lpc.grounded) {
                worlds::AudioSystem::getInstance()->playOneShotClip(jumpSound, t.position, false, 0.6f);
            } else if (lpc.canWallJump) {
                worlds::AudioSystem::getInstance()->playOneShotClip(wallJumpSound, t.position, false, 0.6f);
            }
        }

        if (!dblJumpUsedLast && lpc.doubleJumpUsed) {
            auto& t = registry.get<Transform>(locosphereEnt);
            static int lastSoundIdx = 0;
            int soundIdx = 0;
            while (lastSoundIdx == soundIdx) soundIdx = pcg32_boundedrand_r(&rng, doubleJumpSounds.size());
            lastSoundIdx = soundIdx;
            worlds::AudioSystem::getInstance()->playOneShotClip(doubleJumpSounds[soundIdx], t.position, false, 0.25f);
        }

        if (!groundedLast && lpc.grounded) {
            auto& t = registry.get<Transform>(locosphereEnt);
            worlds::AudioSystem::getInstance()->playOneShotClip(landSound, t.position, false, 0.5f);
        }

        dblJumpUsedLast = lpc.doubleJumpUsed;
        groundedLast = lpc.grounded;

        static float stepTimer = 0.0f;

        if (lpc.grounded) {
            float inputMagnitude = glm::length(lpc.xzMoveInput);
            inputMagnitude *= lpc.sprint ? 1.5f : 1.0f;
            stepTimer += inputMagnitude * deltaTime * 2.0f;
        }

        if (stepTimer >= 1.0f) {
            auto& locosphereTransform = registry.get<Transform>(locosphereEnt);

            // make sure we don't repeat either the last sound or the sound before that
            static int lastSoundIdx = 0;
            static int lastLastSoundIdx = 0;
            int soundIdx = 0;

            // just keep generating numbers until we get an index
            // meeting that criteria
            while (lastSoundIdx == soundIdx || soundIdx == lastLastSoundIdx)
                soundIdx = pcg32_boundedrand_r(&rng, footstepSounds.size());

            lastLastSoundIdx = lastSoundIdx;
            lastSoundIdx = soundIdx;

            worlds::AudioSystem::getInstance()->playOneShotClip(
                footstepSounds[soundIdx],
                locosphereTransform.position,
                false, 0.5f
            );

            stepTimer = 0.0f;
        }
    }

    glm::vec3 LocospherePlayerSystem::calcHeadbobPosition(glm::vec3 desiredVel, glm::vec3 camPos, float deltaTime, bool grounded) {
        static float headbobTime = 0.0f;
        headbobTime += deltaTime;

        static HeadBobSettings settings;
        static float sprintLerp = 0.0f;

        if (headBobDbg && ImGui::Begin("Head bob")) {
            ImGui::InputFloat("Speed Y", &settings.bobSpeed.x);
            ImGui::InputFloat("Speed X", &settings.bobSpeed.y);
            ImGui::InputFloat("Amount Y", &settings.bobAmount.y);
            ImGui::InputFloat("Amount X", &settings.bobAmount.x);
            ImGui::InputFloat("Overall Speed", &settings.overallSpeed);
            ImGui::Text("headbobTime: %.3f", headbobTime);
            ImGui::Text("sprintLerp: %.3f", sprintLerp);
        }

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
                else
                    logWarn("more than one local locosphere!");
            }
        });

        if (!reg.valid(localLocosphereEnt)) {
            return;
        }

        if (!vrInterface) {
            lookX += (float)(inputManager->getMouseDelta().x) * 0.005f * mouseSensitivity.getFloat();
            lookY += (float)(inputManager->getMouseDelta().y) * 0.005f * mouseSensitivity.getFloat();

            lookY = glm::clamp(lookY, -glm::half_pi<float>() + 0.001f, glm::half_pi<float>() - 0.001f);

            camera->rotation = glm::angleAxis(-lookX, glm::vec3(0.0f, 1.0f, 0.0f)) *
                               glm::angleAxis(lookY, glm::vec3(1.0f, 0.0f, 0.0f));
        }

        camera->position = glm::mix(lastCamPos, nextCamPos, interpAlpha);

#ifndef NDEBUG
        ImGui::Text("lcp: %.3f, %.3f, %.3f", lastCamPos.x, lastCamPos.y, lastCamPos.z);
        ImGui::Text("ncp: %.3f, %.3f, %.3f", nextCamPos.x, nextCamPos.y, nextCamPos.z);
        ImGui::Text("interpalpha: %.3f", interpAlpha);
        ImGui::Text("cp: %.3f, %.3f, %.3f", camera->position.x, camera->position.y, camera->position.z);
#endif

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
            desiredVel = glm::vec3(locIn.x, 0.0f, -locIn.y);
        }

        float maxLength = glm::max(glm::abs(desiredVel.x), glm::max(glm::abs(desiredVel.y), glm::abs(desiredVel.z)));
        if (glm::length2(desiredVel) > 0.0f) {
            desiredVel = glm::normalize(desiredVel) * maxLength;
        }

        glm::mat4 camMat;
        if (vrInterface) {
            camMat = vrInterface->getHeadTransform();
        } else {
            camMat = glm::rotate(glm::mat4(1.0f), -lookX, glm::vec3(0.0f, 1.0f, 0.0f));
        }

        desiredVel = camMat * glm::vec4(desiredVel, 0.0f);

        if (vrInterface)
            desiredVel = -desiredVel * glm::inverse(camera->rotation);
        desiredVel.y = 0.0f;

        if (glm::length2(desiredVel) > 0.0f) {
            desiredVel = glm::normalize(desiredVel) * maxLength;
        }

        auto& localLpc = registry.get<LocospherePlayerComponent>(localLocosphereEnt);
        localLpc.xzMoveInput = glm::vec2 { desiredVel.x, desiredVel.z };

        localLpc.sprint = (vrInterface && vrInterface->getSprintInput()) || (inputManager->keyHeld(SDL_SCANCODE_LSHIFT));

        updatePlayerSounds(localLocosphereEnt, deltaTime);

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

                auto& rig = reg.get<PlayerRig>(localLocosphereEnt);
                float invMassScale = rig.fenderJoint->getInvMassScale0();
                if (ImGui::DragFloat("Inv Mass Scale", &invMassScale)) {
                    rig.fenderJoint->setInvMassScale0(invMassScale);
                    rig.fenderJoint->setInvInertiaScale0(invMassScale);
                }

                auto fenderActor = static_cast<physx::PxRigidDynamic*>(registry.get<worlds::DynamicPhysicsActor>(rig.fender).actor);

                auto& wActor = registry.get<worlds::DynamicPhysicsActor>(localLocosphereEnt);
                auto* locosphereActor = (physx::PxRigidDynamic*)wActor.actor;

                float fenderMass = fenderActor->getMass();
                float sphereMass = locosphereActor->getMass();

                if (ImGui::DragFloat("Fender mass", &fenderMass)) {
                    physx::PxRigidBodyExt::setMassAndUpdateInertia(*fenderActor, fenderMass);
                }

                if (ImGui::DragFloat("Sphere mass", &sphereMass)) {
                    physx::PxRigidBodyExt::setMassAndUpdateInertia(*fenderActor, sphereMass);
                }
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

            snprintf(buf, 32, "total vel: %.3fms^-1", lld.linVels[adjIdx]);
            snprintf(buf2, 32, "xz vel: %.3fms^-1", lld.linVelsXZ[adjIdx]);

            auto& io = ImGui::GetIO();
            float fontSize = io.Fonts->Fonts[0]->FontSize;
            ImVec2 textSize = ImGui::CalcTextSize(buf);
            ImVec2 textSize2 = ImGui::CalcTextSize(buf2);

            ImVec2 pos1 { size.x - textSize.x * 0.5f, size.y };
            pos1.x += ImGui::GetMainViewport()->Pos.x;
            pos1.y += ImGui::GetMainViewport()->Pos.y;

            ImVec2 pos2 { size.x - textSize2.x * 0.5f, size.y - fontSize };
            pos2.x += ImGui::GetMainViewport()->Pos.x;
            pos2.y += ImGui::GetMainViewport()->Pos.y;

            ImGui::GetBackgroundDrawList()->AddText(pos1, ImColor(1.0f, 1.0f, 1.0f, 1.0f), buf);
            ImGui::GetBackgroundDrawList()->AddText(pos2, ImColor(1.0f, 1.0f, 1.0f, 1.0f), buf2);
        }

        // dot in the centre of the screen
        if (!vrInterface) {
            auto size = ImGui::GetMainViewport()->Size;
            auto offset = ImGui::GetMainViewport()->Pos;
            ImGui::GetBackgroundDrawList()->AddCircleFilled(
                ImVec2(offset.x + size.x * 0.5f, offset.y + size.y * 0.5f), 2.5f, ImColor(1.0f, 1.0f, 1.0f), 16);
        }
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
            static worlds::ConVar maxSpeed { "lg_locosphereSpeed", "25.0" };
            auto& wActor = registry.get<worlds::DynamicPhysicsActor>(ent);
            auto* locosphereActor = (physx::PxRigidDynamic*)wActor.actor;
            auto& rig = registry.get<PlayerRig>(ent);
            auto locosphereTransform = worlds::px2glm(locosphereActor->getGlobalPose());

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
            if (!glm::any(glm::isnan(torque)))
                locosphereActor->addTorque(worlds::glm2px(torque), physx::PxForceMode::eACCELERATION);

            currVel = worlds::px2glm(locosphereActor->getAngularVelocity());

            NullPhysXCallback nullCallback{};
            lpc.grounded = worlds::g_scene->raycast(worlds::glm2px(locosphereTransform.position - glm::vec3(0.0f, LOCOSPHERE_RADIUS - 0.01f, 0.0f)), physx::PxVec3{ 0.0f, -1.0f, 0.0f }, LOCOSPHERE_RADIUS, nullCallback, physx::PxHitFlag::eDEFAULT, physx::PxQueryFilterData{ physx::PxQueryFlag::ePOSTFILTER | physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::eSTATIC }, &filterEnt);

            glm::vec3 currLinVel = worlds::px2glm(locosphereActor->getLinearVelocity());

            auto& lDebugInfo = locosphereDebug.at(ent);

            lDebugInfo.angVels[physDbgIdx] = glm::length(currVel);
            lDebugInfo.linVels[physDbgIdx] = glm::length(currLinVel);
            lDebugInfo.linVelsXZ[physDbgIdx] = glm::length(glm::vec3{ currLinVel.x, 0.0f, currLinVel.z });

            auto fenderActor = static_cast<physx::PxRigidDynamic*>(registry.get<worlds::DynamicPhysicsActor>(rig.fender).actor);

            if (!lpc.grounded) {
                glm::vec3 airVel { lpc.xzMoveInput.x, 0.0f, lpc.xzMoveInput.y };
                // I don't know why, but multiplying the angular speed by 0.16 gives linear speed.
                airVel *= (float)maxSpeed * 0.16f * (lpc.sprint + 1.0f);
                glm::vec3 addedVel = (airVel - currLinVel);
                addedVel.y = 0.0f;
                locosphereActor->addForce(worlds::glm2px(addedVel), physx::PxForceMode::eACCELERATION);
                fenderActor->addForce(worlds::glm2px(addedVel), physx::PxForceMode::eACCELERATION);
            }

            bool wallPresent = false;
            auto fenderPose = fenderActor->getGlobalPose();
            glm::vec3 wallNormal;

            glm::vec3 left = worlds::px2glm(fenderPose.q) * glm::vec3(-1.0f, 0.0f, 0.0f);
            glm::vec3 right = worlds::px2glm(fenderPose.q) * glm::vec3(1.0f, 0.0f, 0.0f);
            glm::vec3 rayCenter = worlds::px2glm(fenderPose.p) + glm::vec3{ 0.0f, 0.4f, 0.0f };

            worlds::RaycastHitInfo rhi;

            if (worlds::raycast(rayCenter + (left * 0.25f), left, 0.4f, &rhi)) {
                wallPresent = true;
                wallNormal = rhi.normal;
                lpc.canWallJump = true;
            } else if (worlds::raycast(rayCenter + (right * 0.25f), right, 0.4f, &rhi)) {
                wallNormal = rhi.normal;
                wallPresent = true;
                lpc.canWallJump = true;
            } else {
                lpc.canWallJump = false;
            }

            if (lpc.jump) {
                static worlds::ConVar jumpForce{ "jumpForce", "5.0" };
                static worlds::ConVar wallJumpForce{ "wallJumpForce", "10.0" };
                if (lpc.grounded) {
                    locosphereActor->addForce(physx::PxVec3{ 0.0f, jumpForce.getFloat(), 0.0f }, physx::PxForceMode::eVELOCITY_CHANGE);
                    fenderActor->addForce(physx::PxVec3{ 0.0f, jumpForce.getFloat(), 0.0f }, physx::PxForceMode::eVELOCITY_CHANGE);
                } else if (wallPresent) {
                    wallNormal += glm::vec3{ 0.0f, 1.0f, 0.0f };
                    wallNormal = glm::normalize(wallNormal);
                    wallNormal *= wallJumpForce.getFloat();
                    locosphereActor->addForce(worlds::glm2px(wallNormal), physx::PxForceMode::eVELOCITY_CHANGE);
                    fenderActor->addForce(worlds::glm2px(wallNormal), physx::PxForceMode::eVELOCITY_CHANGE);
                } else if (!lpc.doubleJumpUsed) {
                    lpc.doubleJumpUsed = true;
                    locosphereActor->addForce(physx::PxVec3{ 0.0f, jumpForce.getFloat(), 0.0f }, physx::PxForceMode::eVELOCITY_CHANGE);
                    fenderActor->addForce(physx::PxVec3{ 0.0f, jumpForce.getFloat(), 0.0f }, physx::PxForceMode::eVELOCITY_CHANGE);
                }
                lpc.jump = false;
            }

            if (lpc.grounded) {
                lpc.doubleJumpUsed = false;
            }

            if (lpc.isLocal) {
                auto& fenderWorldsTf = registry.get<Transform>(rig.fender);
                auto lspherePose = locosphereActor->getGlobalPose();
                auto fenderPose = fenderActor->getGlobalPose();
                if (!fenderPose.p.isFinite() || !lspherePose.p.isFinite()) {
                    fenderPose = physx::PxTransform{ physx::PxIdentity };

                    fenderWorldsTf.position = glm::vec3{ 0.0f };
                    fenderWorldsTf.rotation = glm::quat{};

                    lspherePose = physx::PxTransform{ physx::PxIdentity };

                    locosphereTransform.position = glm::vec3{ 0.0f };
                    locosphereTransform.rotation = glm::quat{};

                    fenderActor->setGlobalPose(fenderPose);
                    locosphereActor->setGlobalPose(lspherePose);
                    fenderActor->setLinearVelocity(physx::PxVec3{ 0.0f });
                    locosphereActor->setLinearVelocity(physx::PxVec3{ 0.0f });
                    fenderActor->setAngularVelocity(physx::PxVec3{ 0.0f });
                    locosphereActor->setAngularVelocity(physx::PxVec3{ 0.0f });
                }

                lastCamPos = nextCamPos;

                glm::mat4 camMat;
                if (vrInterface) {
                    camMat = vrInterface->getHeadTransform();
                } else {
                    camMat = glm::rotate(glm::mat4(1.0f), -lookX, glm::vec3(0.0f, 1.0f, 0.0f));
                }

                glm::vec3 camOffset{0.0f};

                if (vrInterface) {
                    static glm::vec3 lastHeadPos = glm::vec3{ 0.0f };
                    glm::mat4 headTransform = vrInterface->getHeadTransform();
                    glm::vec3 headPos = worlds::getMatrixTranslation(vrInterface->getHeadTransform());
                    camOffset = worlds::getMatrixRotation(headTransform) * glm::vec3(0.0f, 0.0f, 0.2f);
                    camOffset.y = 0.0f;

                    headPos += camOffset;
                    glm::vec3 locosphereOffset = lastHeadPos - headPos;
                    lastHeadPos = headPos;
                    locosphereOffset.y = 0.0f;

                    locosphereOffset = camera->rotation * locosphereOffset;

                    lspherePose.p += worlds::glm2px(locosphereOffset);
                    locosphereTransform.position += locosphereOffset;
                    locosphereActor->setGlobalPose(lspherePose);

                    fenderPose.p += worlds::glm2px(locosphereOffset);
                }

                glm::vec3 forward = camMat * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
                if (vrInterface) // account for snap turn in VR
                    forward = camera->rotation * forward;
                forward.y = 0.0f;
                forward = glm::normalize(forward);
                fenderPose.q = worlds::glm2px(glm::quatLookAt(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
                fenderActor->setGlobalPose(fenderPose);

                nextCamPos = worlds::px2glm(lspherePose.p + physx::PxVec3(0.0f, -LOCOSPHERE_RADIUS, 0.0f));
                if (glm::any(glm::isnan(nextCamPos)))
                    nextCamPos = glm::vec3{0.0f};

                if (!vrInterface) {
                    // Make all non-VR users 1.65m tall
                    // Then subtract 5cm from that to account for eye offset
                    nextCamPos += glm::vec3(0.0f, 1.6f, 0.0f);
                } else {
                    // Cancel out the movement of the head
                    glm::vec3 headPos = camera->rotation * (worlds::getMatrixTranslation(vrInterface->getHeadTransform()) + camOffset);
                    nextCamPos += glm::vec3{ headPos.x, 0.0f, headPos.z };
                }

                if (!fenderPose.p.isFinite()) {
                    logErr("fender pose invalid");
                    fenderPose.p = physx::PxVec3{ 0.0f };
                }

                if (!lspherePose.p.isFinite()) {
                    logErr("lsphere pose invalid");
                    lspherePose.p = physx::PxVec3{ 0.0f };
                }

                physDbgIdx++;
                if (physDbgIdx == 128)
                    physDbgIdx = 0;
                lpc.headPos = nextCamPos;
            }
        });
    }

    PlayerRig LocospherePlayerSystem::createPlayerRig(entt::registry& registry, glm::vec3 position) {
        // Locosphere
        auto playerLocosphere = registry.create();
        auto& pTransform = registry.emplace<Transform>(playerLocosphere);
        pTransform.position = position;
        pTransform.scale = glm::vec3{0.66f};
        registry.emplace<LocospherePlayerComponent>(playerLocosphere).isLocal = true;
        registry.emplace<worlds::NameComponent>(playerLocosphere, "Locosphere");
        registry.emplace<worlds::WorldObject>(playerLocosphere, worlds::AssetDB::pathToId("Materials/dev.json"), worlds::AssetDB::pathToId("Models/Sphere.wmdl"));

        auto actor = worlds::g_physics->createRigidDynamic(physx::PxTransform{ worlds::glm2px(position), physx::PxQuat{physx::PxIdentity} });
        auto& wActor = registry.emplace<worlds::DynamicPhysicsActor>(playerLocosphere, actor);

        actor->setSolverIterationCounts(16, 4);
        worlds::g_scene->addActor(*actor);

        locosphereMat = worlds::g_physics->createMaterial(15.0f, 15.0f, 0.2f);
        locosphereMat->setFrictionCombineMode(physx::PxCombineMode::eMAX);
        wActor.physicsShapes.push_back(worlds::PhysicsShape::sphereShape(LOCOSPHERE_RADIUS, locosphereMat));

        wActor.mass = 40.0f;
        wActor.scaleShapes = false;
        wActor.layer = worlds::PLAYER_PHYSICS_LAYER;
        worlds::updatePhysicsShapes(wActor);
        worlds::updateMass(wActor);

        // Set up player fender and joint
        auto playerFender = registry.create();
        registry.emplace<Transform>(playerFender).position = position + glm::vec3{0.0f, 0.4f, 0.0f};
        registry.emplace<worlds::NameComponent>(playerFender, "Fender");

        auto fenderActor = worlds::g_physics->createRigidDynamic(physx::PxTransform{
                worlds::glm2px(position + glm::vec3{0.0f ,0.4f, 0.0f}), physx::PxQuat{physx::PxIdentity} });
        auto& fenderWActor = registry.emplace<worlds::DynamicPhysicsActor>(playerFender, fenderActor);
        fenderWActor.scaleShapes = false;

        worlds::g_scene->addActor(*fenderActor);

        fenderMat = worlds::g_physics->createMaterial(0.0f, 0.0f, 0.0f);
        auto bodyShape = worlds::PhysicsShape::capsuleShape(0.15f, 1.0f, fenderMat);
        bodyShape.pos = glm::vec3(0.0f, 0.1f, 0.0f);
        bodyShape.rot = glm::quat(glm::vec3(0.0f, 0.0f, glm::half_pi<float>()));
        fenderWActor.physicsShapes.push_back(bodyShape);

        auto headShape = worlds::PhysicsShape::sphereShape(0.25f, fenderMat);
        headShape.pos = glm::vec3(0.0f, 0.75f, 0.0f);
        fenderWActor.physicsShapes.push_back(headShape);

        auto fenderShape = worlds::PhysicsShape::sphereShape(LOCOSPHERE_RADIUS + 0.05f);
        fenderShape.pos = glm::vec3(0.0f, -LOCOSPHERE_RADIUS - 0.03f, 0.0f);
        fenderWActor.physicsShapes.push_back(fenderShape);

        fenderWActor.mass = 3.0f;
        fenderWActor.layer = worlds::PLAYER_PHYSICS_LAYER;

        worlds::updatePhysicsShapes(fenderWActor);
        worlds::updateMass(fenderWActor);

        auto& d6Comp = registry.emplace<worlds::D6Joint>(playerLocosphere);

        d6Comp.setTarget(playerFender, registry);

        physx::PxTransform offset{ physx::PxIdentity };
        offset.p = physx::PxVec3{ 0.0f, -0.4f, 0.0f };
        d6Comp.pxJoint->setLocalPose(physx::PxJointActorIndex::eACTOR1, offset);

        fenderActor->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_X, true);
        fenderActor->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y, true);
        fenderActor->setRigidDynamicLockFlag(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z, true);

        auto fenderJoint = d6Comp.pxJoint;

        fenderJoint->setMotion(physx::PxD6Axis::eX, physx::PxD6Motion::eLOCKED);
        fenderJoint->setMotion(physx::PxD6Axis::eY, physx::PxD6Motion::eLOCKED);
        fenderJoint->setMotion(physx::PxD6Axis::eZ, physx::PxD6Motion::eLOCKED);

        fenderJoint->setMotion(physx::PxD6Axis::eSWING1, physx::PxD6Motion::eFREE);
        fenderJoint->setMotion(physx::PxD6Axis::eSWING2, physx::PxD6Motion::eFREE);
        fenderJoint->setMotion(physx::PxD6Axis::eTWIST, physx::PxD6Motion::eFREE);

        fenderJoint->setConstraintFlag(physx::PxConstraintFlag::eCOLLISION_ENABLED, false);
        //fenderActor->setSolverIterationCounts(32, 8);

        logMsg("locosphere entity is %u", (uint32_t)playerLocosphere);

        PlayerRig rig;
        rig.locosphere = playerLocosphere;
        rig.fender = playerFender;
        rig.fenderJoint = fenderJoint;
        registry.emplace<PlayerRig>(playerLocosphere, rig);

        auto lspherePos = worlds::glm2px(position);
        auto fenderPos = worlds::glm2px(position + glm::vec3 { 0.0f, 0.4f, 0.0f });

        actor->setGlobalPose(physx::PxTransform{ lspherePos, physx::PxQuat { physx::PxIdentity} });
        fenderActor->setGlobalPose(physx::PxTransform{ fenderPos, physx::PxQuat { physx::PxIdentity} });

        return rig;
    }

    void LocospherePlayerSystem::onSceneStart(entt::registry&) {
        // Create physics rig
        lspherePid.P = 75.0f;
        lspherePid.I = 0.0f;
        lspherePid.D = 0.0f;
        zeroThresh = 0.0f;
    }

    void LocospherePlayerSystem::shutdown(entt::registry&) {
    }
}
