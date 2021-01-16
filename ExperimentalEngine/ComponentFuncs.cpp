#include "ComponentFuncs.hpp"
#include <entt/entt.hpp>
#include "imgui.h"
#include "Transform.hpp"
#include "IconsFontaudio.h"
#include "IconsFontAwesome5.h"
#include <glm/gtc/type_ptr.hpp>
#include "Engine.hpp"
#include "GuiUtil.hpp"
#include <physx/foundation/PxTransform.h>
#include "Physics.hpp"
#include "NameComponent.hpp"
#include "Audio.hpp"
#include "Render.hpp"

namespace worlds {
    void editTransform(entt::entity ent, entt::registry& reg) {
        if (ImGui::CollapsingHeader(ICON_FA_ARROWS_ALT u8" Transform")) {
            auto& selectedTransform = reg.get<Transform>(ent);
            ImGui::DragFloat3("Position", &selectedTransform.position.x);

            glm::vec3 eulerRot = glm::degrees(glm::eulerAngles(selectedTransform.rotation));
            if (ImGui::DragFloat3("Rotation", glm::value_ptr(eulerRot))) {
                selectedTransform.rotation = glm::radians(eulerRot);
            }

            ImGui::DragFloat3("Scale", &selectedTransform.scale.x);
            ImGui::Separator();
        }
    }

    void editWorldObject(entt::entity ent, entt::registry& reg) {
        if (ImGui::CollapsingHeader(ICON_FA_PENCIL_ALT u8" WorldObject")) {
            if (ImGui::Button("Remove##WO")) {
                reg.remove<WorldObject>(ent);
            } else {
                auto& worldObject = reg.get<WorldObject>(ent);
                ImGui::DragFloat2("Texture Scale", &worldObject.texScaleOffset.x);
                ImGui::DragFloat2("Texture Offset", &worldObject.texScaleOffset.z);

                for (int i = 0; i < NUM_SUBMESH_MATS; i++) {
                    if (worldObject.presentMaterials[i]) {
                        ImGui::Text("Material %i: %s", i, g_assetDB.getAssetPath(worldObject.materials[i]).c_str());

                    } else {
                        ImGui::Text("Material %i: not set", i);
                    }

                    ImGui::SameLine();

                    std::string idStr = "##" + std::to_string(i);

                    bool open = ImGui::Button(("Change" + idStr).c_str());
                    if (selectAssetPopup(("Material" + idStr).c_str(), worldObject.materials[i], open)) {
                        worldObject.materialIdx[i] = ~0u;
                        worldObject.presentMaterials[i] = true;
                    }
                }
            }

            ImGui::Separator();
        }
    }

    const std::unordered_map<LightType, const char*> lightTypeNames = {
            { LightType::Directional, "Directional" },
            { LightType::Point, "Point" },
            { LightType::Spot, "Spot" }
    };

    void createLight(entt::entity ent, entt::registry& reg) {
        reg.emplace<WorldLight>(ent);
    }

    void editLight(entt::entity ent, entt::registry& reg) {
        if (ImGui::CollapsingHeader(ICON_FA_LIGHTBULB u8" Light")) {
            if (ImGui::Button("Remove##WL")) {
                reg.remove<WorldLight>(ent);
            } else {
                auto& worldLight = reg.get<WorldLight>(ent);
                ImGui::ColorEdit3("Color", &worldLight.color.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);

                if (ImGui::BeginCombo("Light Type", lightTypeNames.at(worldLight.type))) {
                    for (auto& p : lightTypeNames) {
                        bool isSelected = worldLight.type == p.first;
                        if (ImGui::Selectable(p.second, &isSelected)) {
                            worldLight.type = p.first;
                        }

                        if (isSelected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                if (worldLight.type == LightType::Spot) {
                    ImGui::DragFloat("Spot Cutoff", &worldLight.spotCutoff);
                }
            }
        }
    }

    void cloneLight(entt::entity from, entt::entity to, entt::registry& reg) {
        reg.emplace<WorldLight>(to, reg.get<WorldLight>(from));
    }

    void createPhysicsActor(entt::entity ent, entt::registry& reg) {
        auto& t = reg.get<Transform>(ent);

        physx::PxTransform pTf(glm2px(t.position), glm2px(t.rotation));
        auto* actor = g_physics->createRigidStatic(pTf);
        reg.emplace<PhysicsActor>(ent, actor);
        g_scene->addActor(*actor);
    }

    void createDynamicPhysicsActor(entt::entity ent, entt::registry& reg) {
        auto& t = reg.get<Transform>(ent);

        physx::PxTransform pTf(glm2px(t.position), glm2px(t.rotation));
        auto* actor = g_physics->createRigidDynamic(pTf);
        reg.emplace<DynamicPhysicsActor>(ent, actor);
        g_scene->addActor(*actor);
    }

    const char* shapeTypeNames[(int)PhysicsShapeType::Count] = {
        "Sphere",
        "Box",
        "Capsule",
        "Mesh"
    };

    template <typename T>
    void editPhysicsShapes(T& actor) {
        ImGui::Text("Shapes: %zu", actor.physicsShapes.size());

        ImGui::SameLine();

        if (ImGui::Button("Add")) {
            actor.physicsShapes.push_back(PhysicsShape::boxShape(glm::vec3(0.5f)));
        }

        std::vector<PhysicsShape>::iterator eraseIter;
        bool erase = false;

        int i = 0;
        for (auto it = actor.physicsShapes.begin(); it != actor.physicsShapes.end(); it++) {
            ImGui::PushID(i);
            if (ImGui::BeginCombo("Collider Type", shapeTypeNames[(int)it->type])) {
                for (int iType = 0; iType < (int)PhysicsShapeType::Count; iType++) {
                    auto type = (PhysicsShapeType)iType;
                    bool isSelected = it->type == type;
                    if (ImGui::Selectable(shapeTypeNames[iType], &isSelected)) {
                        it->type = type;
                    }

                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::SameLine();
            if (ImGui::Button("Remove")) {
                eraseIter = it;
                erase = true;
            }

            ImGui::DragFloat3("Position", &it->pos.x);

            switch (it->type) {
            case PhysicsShapeType::Sphere:
                ImGui::DragFloat("Radius", &it->sphere.radius);
                break;
            case PhysicsShapeType::Box:
                ImGui::DragFloat3("Half extents", &it->box.halfExtents.x);
                break;
            case PhysicsShapeType::Capsule:
                ImGui::DragFloat("Height", &it->capsule.height);
                ImGui::DragFloat("Radius", &it->capsule.radius);
                break;
            default: break;
            }
            ImGui::PopID();
            i++;
        }

        if (erase)
            actor.physicsShapes.erase(eraseIter);
    }

    void editPhysicsActor(entt::entity ent, entt::registry& reg) {
        auto& pa = reg.get<PhysicsActor>(ent);
        if (ImGui::CollapsingHeader(ICON_FA_SHAPES u8" Physics Actor")) {
            if (ImGui::Button("Remove##PA")) {
                reg.remove<PhysicsActor>(ent);
            } else {
                if (ImGui::Button("Update Collisions")) {
                    updatePhysicsShapes(pa);
                }

                editPhysicsShapes(pa);
            }

            ImGui::Separator();
        }
    }

    void editDynamicPhysicsActor(entt::entity ent, entt::registry& reg) {
        auto& pa = reg.get<DynamicPhysicsActor>(ent);
        if (ImGui::CollapsingHeader(ICON_FA_SHAPES u8" Dynamic Physics Actor")) {
            if (ImGui::Button("Remove##DPA")) {
                reg.remove<DynamicPhysicsActor>(ent);
            } else {
                ImGui::DragFloat("Mass", &pa.mass);
                if (ImGui::Button("Update Collisions##DPA")) {
                    updatePhysicsShapes(pa);
                    physx::PxRigidBodyExt::updateMassAndInertia(*((physx::PxRigidDynamic*)pa.actor), &pa.mass, 1);
                }

                editPhysicsShapes(pa);
            }

            ImGui::Separator();
        }
    }

    void clonePhysicsActor(entt::entity a, entt::entity b, entt::registry& reg) {
        auto& t = reg.get<Transform>(b);

        physx::PxTransform pTf(glm2px(t.position), glm2px(t.rotation));
        auto* actor = g_physics->createRigidStatic(pTf);

        auto& newPhysActor = reg.emplace<PhysicsActor>(b, actor);
        newPhysActor.physicsShapes = reg.get<PhysicsActor>(a).physicsShapes;

        g_scene->addActor(*actor);

        updatePhysicsShapes(newPhysActor);
    }

    void cloneDynamicPhysicsActor(entt::entity a, entt::entity b, entt::registry& reg) {
        auto& t = reg.get<Transform>(b);

        physx::PxTransform pTf(glm2px(t.position), glm2px(t.rotation));
        auto* actor = g_physics->createRigidDynamic(pTf);

        auto& newPhysActor = reg.emplace<DynamicPhysicsActor>(b, actor);
        newPhysActor.physicsShapes = reg.get<DynamicPhysicsActor>(a).physicsShapes;

        g_scene->addActor(*actor);

        updatePhysicsShapes(newPhysActor);
    }

    void editNameComponent(entt::entity ent, entt::registry& registry) {
        auto& nc = registry.get<NameComponent>(ent);

        ImGui::InputText("Name", &nc.name);
        ImGui::SameLine();
        if (ImGui::Button("Remove")) {
            registry.remove<NameComponent>(ent);
        }
        ImGui::Separator();
    }

    void createNameComponent(entt::entity ent, entt::registry& registry) {
        registry.emplace<NameComponent>(ent);
    }

    void cloneNameComponent(entt::entity a, entt::entity b, entt::registry& reg) {
        reg.emplace<NameComponent>(b, reg.get<NameComponent>(a));
    }

    void editAudioSource(entt::entity ent, entt::registry& registry) {
        auto& as = registry.get<AudioSource>(ent);

        if (ImGui::CollapsingHeader(ICON_FAD_SPEAKER u8" Audio Source")) {
            ImGui::Checkbox("Loop", &as.loop);
            ImGui::Checkbox("Spatialise", &as.spatialise);
            ImGui::Checkbox("Play on scene open", &as.playOnSceneOpen);
            ImGui::Text("Current Asset Path: %s", g_assetDB.getAssetPath(as.clipId).c_str());

            selectAssetPopup("Audio Source Path", as.clipId, ImGui::Button("Change"));

            if (ImGui::Button(ICON_FA_PLAY u8" Preview"))
                AudioSystem::getInstance()->playOneShotClip(as.clipId, glm::vec3(0.0f));

            ImGui::Separator();
        }
    }

    void createAudioSource(entt::entity ent, entt::registry& reg) {
        reg.emplace<AudioSource>(ent, g_assetDB.addOrGetExisting("Audio/SFX/dlgsound.ogg"));
    }

    void cloneAudioSource(entt::entity a, entt::entity b, entt::registry& reg) {
        auto& asA = reg.get<AudioSource>(a);

        reg.emplace<AudioSource>(a, asA);
    }

    void createWorldCubemap(entt::entity ent, entt::registry& reg) {
        auto& wc = reg.emplace<WorldCubemap>(ent);

        wc.cubemapId = g_assetDB.addOrGetExisting("DefaultCubemap.json");
        wc.extent = glm::vec3{ 1.0f };
    }

    void editWorldCubemap(entt::entity ent, entt::registry& reg) {
        auto& wc = reg.get<WorldCubemap>(ent);

        if (ImGui::CollapsingHeader(ICON_FA_CIRCLE u8" Cubemap")) {
            ImGui::DragFloat3("Extent", &wc.extent.x);
            ImGui::Text("Current Asset Path: %s", g_assetDB.getAssetPath(wc.cubemapId).c_str());
            AssetID oldId = wc.cubemapId;
            selectAssetPopup("Cubemap Path", wc.cubemapId, ImGui::Button("Change"));

            if (wc.cubemapId != oldId) {
                wc.loadIdx = ~0u;
            }
            ImGui::Separator();
        }
    }
}