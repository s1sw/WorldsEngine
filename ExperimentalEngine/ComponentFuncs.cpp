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
#include "sajson.h"
#include "JsonUtil.hpp"
#include "D6Joint.hpp"

namespace worlds {
    ComponentEditorLink* ComponentEditor::first = nullptr;

    ComponentEditor::ComponentEditor() {
        if (!first) {
            first = new ComponentEditorLink;
        } else {
            ComponentEditorLink* next = first;
            first = new ComponentEditorLink;
            first->next = next;
        }

        first->editor = this;
    }

    template <typename T>
    class BasicComponentUtil : public ComponentEditor {
        bool allowInspectorAdd() override {
            return true;
        }

        ENTT_ID_TYPE getComponentID() override {
            return entt::type_info<T>::id();
        }

        void create(entt::entity ent, entt::registry& reg) override {
            reg.emplace<T>(ent);
        }

        void clone(entt::entity from, entt::entity to, entt::registry& reg) override {
            reg.emplace<T>(to, reg.get<T>(from));
        }
    };

    class TransformEditor : public BasicComponentUtil<Transform> {
    public:
        const char* getName() override {
            return "Transform";
        }

        bool allowInspectorAdd() override {
            return false;
        }

        void edit(entt::entity ent, entt::registry& reg) override {
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
    };


    class WorldObjectEditor : public BasicComponentUtil<WorldObject> {
    public:
        const char* getName() {
            return "World Object";
        }

        void create(entt::entity ent, entt::registry& reg) override {
            auto cubeId = g_assetDB.addOrGetExisting("model.obj");
            auto matId = g_assetDB.addOrGetExisting("Materials/dev.json");
            reg.emplace<WorldObject>(ent, matId, cubeId);
        }

        void edit(entt::entity ent, entt::registry& reg) override {
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
    };

    const std::unordered_map<LightType, const char*> lightTypeNames = {
            { LightType::Directional, "Directional" },
            { LightType::Point, "Point" },
            { LightType::Spot, "Spot" }
    };

    class WorldLightEditor : public BasicComponentUtil<WorldLight> {
    public:
        const char* getName() override { return "World Light"; }
        
        void edit(entt::entity ent, entt::registry& reg) {
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
    };

    const char* shapeTypeNames[(int)PhysicsShapeType::Count] = {
        "Sphere",
        "Box",
        "Capsule",
        "Mesh"
    };

    const sajson::string saStr(const char* str) {
        return sajson::string{ str, strlen(str) };
    }

    PhysicsShapeType valToShapeType(const sajson::value& val) {
        const auto& str = val.as_string();

        if (str == "CUBE")
            return PhysicsShapeType::Box;
        else
            return PhysicsShapeType::Sphere;
    }

    std::vector<PhysicsShape> loadColliderJson(const char* path) {
        auto* file = PHYSFS_openRead(path);
        auto len = PHYSFS_fileLength(file);
        char* buf = (char*)std::malloc(len);

        PHYSFS_readBytes(file, buf, len);
        PHYSFS_close(file);

        const sajson::document& doc = sajson::parse(sajson::single_allocation(), sajson::mutable_string_view(len, buf));


        if (doc.get_root().get_type() != sajson::TYPE_ARRAY)
            fatalErr("what");

        std::vector<PhysicsShape> shapes;
        const auto& root = doc.get_root();
        shapes.resize(root.get_length());

        for (size_t i = 0; i < root.get_length(); i++) {
            PhysicsShape shape;
            const auto& el = root.get_array_element(i);
            const auto& typeval = el.get_value_of_key(sajson::string{ "type", 4 });
            shape.type = valToShapeType(typeval);

            getVec3(el, "position", shape.pos);
            glm::vec3 eulerAngles;
            getVec3(el, "rotation", eulerAngles);
            shape.pos = glm::vec3{ shape.pos.x, shape.pos.z, -shape.pos.y };
            eulerAngles = glm::vec3{ eulerAngles.x, eulerAngles.z, -eulerAngles.y };
            shape.rot = glm::quat{ eulerAngles };

            if (shape.type == PhysicsShapeType::Box) {
                getVec3(el, "scale", shape.box.halfExtents);
                shape.box.halfExtents = glm::vec3{ shape.box.halfExtents.x, shape.box.halfExtents.z, shape.box.halfExtents.y };
            }

            shapes[i] = shape;
        }

        std::free(buf);

        return shapes;
    }

    template <typename T>
    void editPhysicsShapes(T& actor) {
        if (ImGui::Button("Load Collider JSON")) {
            ImGui::OpenPopup("Collider JSON");
        }

        openFileModal("Collider JSON", [&](const char* p) {
            actor.physicsShapes = loadColliderJson(p);
            }, ".json");

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

    class PhysicsActorEditor : public BasicComponentUtil<PhysicsActor> {
    public:
        const char* getName() override { return "Physics Actor"; }

        void create(entt::entity ent, entt::registry& reg) override {
            auto& t = reg.get<Transform>(ent);

            physx::PxTransform pTf(glm2px(t.position), glm2px(t.rotation));
            auto* actor = g_physics->createRigidStatic(pTf);
            reg.emplace<PhysicsActor>(ent, actor);
            g_scene->addActor(*actor);
        }

        void clone(entt::entity from, entt::entity to, entt::registry& reg) override {
            auto& t = reg.get<Transform>(from);

            physx::PxTransform pTf(glm2px(t.position), glm2px(t.rotation));
            auto* actor = g_physics->createRigidStatic(pTf);

            auto& newPhysActor = reg.emplace<PhysicsActor>(to, actor);
            newPhysActor.physicsShapes = reg.get<PhysicsActor>(from).physicsShapes;

            g_scene->addActor(*actor);

            updatePhysicsShapes(newPhysActor);
        }

        void edit(entt::entity ent, entt::registry& reg) override {
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
    };

    class DynamicPhysicsActorEditor : public BasicComponentUtil<DynamicPhysicsActor> {
    public:
        const char* getName() override { return "Dynamic Physics Actor"; }

        void create(entt::entity ent, entt::registry& reg) override {
            auto& t = reg.get<Transform>(ent);

            physx::PxTransform pTf(glm2px(t.position), glm2px(t.rotation));
            auto* actor = g_physics->createRigidDynamic(pTf);
            reg.emplace<DynamicPhysicsActor>(ent, actor);
            g_scene->addActor(*actor);
        }

        void clone(entt::entity from, entt::entity to, entt::registry& reg) override {
            auto& t = reg.get<Transform>(from);

            physx::PxTransform pTf(glm2px(t.position), glm2px(t.rotation));
            auto* actor = g_physics->createRigidStatic(pTf);

            auto& newPhysActor = reg.emplace<PhysicsActor>(to, actor);
            newPhysActor.physicsShapes = reg.get<PhysicsActor>(from).physicsShapes;

            g_scene->addActor(*actor);

            updatePhysicsShapes(newPhysActor);
        }

        void edit(entt::entity ent, entt::registry& reg) override {
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

        void clone(entt::entity from, entt::entity to, entt::registry& reg) override {
            auto& t = reg.get<Transform>(from);

            physx::PxTransform pTf(glm2px(t.position), glm2px(t.rotation));
            auto* actor = g_physics->createRigidDynamic(pTf);

            auto& newPhysActor = reg.emplace<DynamicPhysicsActor>(to, actor);
            newPhysActor.physicsShapes = reg.get<DynamicPhysicsActor>(from).physicsShapes;

            g_scene->addActor(*actor);

            updatePhysicsShapes(newPhysActor);
        }
    };

    class NameComponentEditor : public BasicComponentUtil<NameComponent> {
    public:
        const char* getName() override { return "Name Component"; }

        void edit(entt::entity ent, entt::registry& registry) override {
            auto& nc = registry.get<NameComponent>(ent);

            ImGui::InputText("Name", &nc.name);
            ImGui::SameLine();
            if (ImGui::Button("Remove")) {
                registry.remove<NameComponent>(ent);
            }
            ImGui::Separator();
        }
    };

    class AudioSourceEditor : public BasicComponentUtil<AudioSource> {
    public:
        const char* getName() override { return "Audio Source"; }

        void create(entt::entity ent, entt::registry& reg) override {
            reg.emplace<AudioSource>(ent, g_assetDB.addOrGetExisting("Audio/SFX/dlgsound.ogg"));
        }

        void edit(entt::entity ent, entt::registry& registry) override {
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
    };

    class WorldCubemapEditor : public BasicComponentUtil<WorldCubemap> {
    public:
        const char* getName() override { return "World Cubemap"; }

        void create(entt::entity ent, entt::registry& reg) override {
            auto& wc = reg.emplace<WorldCubemap>(ent);

            wc.cubemapId = g_assetDB.addOrGetExisting("DefaultCubemap.json");
            wc.extent = glm::vec3{ 1.0f };
        }

        void edit(entt::entity ent, entt::registry& reg) override {
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
    };

    const char* motionNames[3] = {
        "Locked",
        "Limited",
        "Free"
    };

    const char* motionAxisLabels[physx::PxD6Axis::eCOUNT] = {
        "X Motion",
        "Y Motion",
        "Z Motion",
        "Twist Motion",
        "Swing 1 Motion",
        "Swing 2 Motion"
    };

    bool motionDropdown(const char* label, physx::PxD6Motion::Enum& val) {
        bool ret = false;
        if (ImGui::BeginCombo(label, motionNames[(int)val])) {
            for (int iType = 0; iType < 3; iType++) {
                auto type = (physx::PxD6Motion::Enum)iType;
                bool isSelected = val == type;
                if (ImGui::Selectable(motionNames[iType], &isSelected)) {
                    val = type;
                    ret = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        return ret;
    }

    class D6JointEditor : public BasicComponentUtil<D6Joint> {
    public:
        const char* getName() override { return "D6 Joint"; }

        void create(entt::entity ent, entt::registry& reg) override {
            if (!reg.has<DynamicPhysicsActor>(ent)) {
                logWarn("Can't add a D6 joint to an entity without a dynamic physics actor!");
                return;
            }

            reg.emplace<D6Joint>(ent);
        }

        void edit(entt::entity ent, entt::registry& reg) override {
            auto& j = reg.get<D6Joint>(ent);
            auto& dpa = reg.get<DynamicPhysicsActor>(ent);

            if (ImGui::CollapsingHeader(ICON_FA_ATOM u8" D6 Joint")) {
                dpa.actor->is<physx::PxRigidDynamic>()->wakeUp();
                for (physx::PxD6Axis::Enum axis = physx::PxD6Axis::eX; axis < physx::PxD6Axis::eCOUNT; ((int&)axis)++) {
                    auto motion = j.pxJoint->getMotion(axis);
                    if (motionDropdown(motionAxisLabels[axis], motion)) {
                        j.pxJoint->setMotion(axis, motion);
                    }
                }

                auto t0 = j.pxJoint->getLocalPose(physx::PxJointActorIndex::eACTOR0);
                auto t1 = j.pxJoint->getLocalPose(physx::PxJointActorIndex::eACTOR1);

                if (ImGui::DragFloat3("Local Offset", &t0.p.x)) {
                    j.pxJoint->setLocalPose(physx::PxJointActorIndex::eACTOR0, t0);
                }

                if (ImGui::DragFloat3("Target Offset", &t1.p.x)) {
                    j.pxJoint->setLocalPose(physx::PxJointActorIndex::eACTOR1, t1);
                }
            }
        }
    };
}