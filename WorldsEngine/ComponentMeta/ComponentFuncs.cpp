#include "ComponentFuncs.hpp"
#include <entt/entt.hpp>
#include "Serialization/SceneSerialization.hpp"
#include "entt/entity/fwd.hpp"
#include "../ImGui/imgui.h"
#include "../Core/Transform.hpp"
#include "../Libs/IconsFontaudio.h"
#include "../Libs/IconsFontAwesome5.h"
#include <glm/gtc/type_ptr.hpp>
#include "../Core/Engine.hpp"
#include "../Editor/GuiUtil.hpp"
#include <foundation/PxTransform.h>
#include "../Physics/Physics.hpp"
#include "../Core/NameComponent.hpp"
#include "../Audio/Audio.hpp"
#include "../Render/Render.hpp"
#include "robin_hood.h"
#include "sajson.h"
#include "../Util/JsonUtil.hpp"
#include "../Physics/D6Joint.hpp"
#include "ComponentEditorUtil.hpp"
#include "../Scripting/ScriptComponent.hpp"
#include "../Util/EnumUtil.hpp"
#include "../Editor/Editor.hpp"
#include <nlohmann/json.hpp>
#include "../UI/WorldTextComponent.hpp"

// Janky workaround to fix static constructors not being called
// (static constructors are only required to be called before the first function in the translation unit)
// (yay for typical c++ specification bullshittery)
#include "D6JointEditor.hpp"

using json = nlohmann::json;

namespace worlds {
#define WRITE_FIELD(file, field) PHYSFS_writeBytes(file, &field, sizeof(field))
#define READ_FIELD(file, field) PHYSFS_readBytes(file, &field, sizeof(field))

    ComponentEditorLink* ComponentEditor::first = nullptr;

    ComponentEditor::ComponentEditor() {
        if (!first) {
            first = new ComponentEditorLink;
            first->next = nullptr;
        } else {
            ComponentEditorLink* next = first;
            first = new ComponentEditorLink;
            first->next = next;
        }

        first->editor = this;
    }

    class TransformEditor : public virtual BasicComponentUtil<Transform> {
    public:
        BASIC_CREATE(Transform);
        BASIC_CLONE(Transform);

        int getSortID() override {
            return -1;
        }

        const char* getName() override {
            return "Transform";
        }

        bool allowInspectorAdd() override {
            return false;
        }

        void edit(entt::entity ent, entt::registry& reg, Editor* ed) override {
            if (ImGui::CollapsingHeader(ICON_FA_ARROWS_ALT u8" Transform")) {
                auto& selectedTransform = reg.get<Transform>(ent);
                glm::vec3 pos = selectedTransform.position;
                if (ImGui::DragFloat3("Position", &pos.x)) {
                    ed->undo.pushState(reg);
                    selectedTransform.position = pos;
                }

                glm::vec3 eulerRot = glm::degrees(glm::eulerAngles(selectedTransform.rotation));
                if (ImGui::DragFloat3("Rotation", glm::value_ptr(eulerRot))) {
                    ed->undo.pushState(reg);
                    selectedTransform.rotation = glm::radians(eulerRot);
                }

                glm::vec3 scale = selectedTransform.scale;
                if (ImGui::DragFloat3("Scale", &scale.x) && !glm::any(glm::equal(scale, glm::vec3{0.0f}))) {
                    selectedTransform.scale = scale;
                }

                if (ImGui::Button("Snap to world grid")) {
                    ed->undo.pushState(reg);
                    selectedTransform.position = glm::round(selectedTransform.position);
                    selectedTransform.scale = glm::round(selectedTransform.scale);
                    eulerRot = glm::round(eulerRot / 15.0f) * 15.0f;
                    selectedTransform.rotation = glm::radians(eulerRot);
                }

                ImGui::SameLine();

                if (ImGui::Button("Snap Rotation")) {
                    ed->undo.pushState(reg);
                    eulerRot = glm::round(eulerRot / 15.0f) * 15.0f;
                    selectedTransform.rotation = glm::radians(eulerRot);
                }

                ImGui::Separator();
            }
        }

        void writeToFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file) override {
            auto& t = reg.get<Transform>(ent);
            WRITE_FIELD(file, t.position);
            WRITE_FIELD(file, t.rotation);
            WRITE_FIELD(file, t.scale);
        }

        void readFromFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file, int) override {
            auto& t = reg.emplace<Transform>(ent);
            READ_FIELD(file, t.position);
            READ_FIELD(file, t.rotation);
            READ_FIELD(file, t.scale);
        }

        void toJson(entt::entity ent, entt::registry& reg, nlohmann::json& j) override {
            const auto& t = reg.get<Transform>(ent);
            j = {
                { "position", t.position },
                { "rotation", t.rotation },
                { "scale", t.scale }
            };
        }

        void fromJson(entt::entity ent, entt::registry& reg, const nlohmann::json& j) override {
            auto& t = reg.emplace<Transform>(ent);
            t.position = j["position"].get<glm::vec3>();
            t.rotation = j["rotation"].get<glm::quat>();
            t.scale = j["scale"].get<glm::vec3>();
        }
    };

    const robin_hood::unordered_flat_map<StaticFlags, const char*> flagNames = {
        { StaticFlags::Audio, "Audio" },
        { StaticFlags::Rendering, "Rendering" },
        { StaticFlags::Navigation, "Navigation" }
    };

    const char* uvOverrideNames[] = {
        "None",
        "XY",
        "XZ",
        "ZY",
        "Pick Best"
    };

    class WorldObjectEditor : public BasicComponentUtil<WorldObject> {
    public:
        BASIC_CLONE(WorldObject);

        const char* getName() override {
            return "World Object";
        }

        void create(entt::entity ent, entt::registry& reg) override {
            auto cubeId = AssetDB::pathToId("model.obj");
            auto matId = AssetDB::pathToId("Materials/dev.json");
            reg.emplace<WorldObject>(ent, matId, cubeId);
        }

        void edit(entt::entity ent, entt::registry& reg, Editor* ed) override {
            if (ImGui::CollapsingHeader(ICON_FA_PENCIL_ALT u8" WorldObject")) {
                if (ImGui::Button("Remove##WO")) {
                    reg.remove<WorldObject>(ent);
                } else {
                    auto& worldObject = reg.get<WorldObject>(ent);
                    if (ImGui::TreeNode("Static Flags")) {
                        for (int i = 1; i < 8; i <<= 1) {
                            bool hasFlag = enumHasFlag(worldObject.staticFlags, (StaticFlags)i);
                            if (ImGui::Checkbox(flagNames.at((StaticFlags)i), &hasFlag)) {
                                int withoutI = (int)worldObject.staticFlags & (~i);
                                withoutI |= i * hasFlag;
                                worldObject.staticFlags = (StaticFlags)withoutI;
                            }
                        }
                        ImGui::TreePop();
                    }

                    if (ImGui::BeginCombo("UV Override", uvOverrideNames[(int)worldObject.uvOverride])) {
                        int i = 0;
                        for (auto& p : uvOverrideNames) {
                            bool isSelected = (int)worldObject.uvOverride == i;
                            if (ImGui::Selectable(p, &isSelected)) {
                                worldObject.uvOverride = (UVOverride)i;
                            }

                            if (isSelected)
                                ImGui::SetItemDefaultFocus();
                            i++;
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::DragFloat2("Texture Scale", &worldObject.texScaleOffset.x);
                    ImGui::DragFloat2("Texture Offset", &worldObject.texScaleOffset.z);

                    ImGui::Text("Mesh: %s", AssetDB::idToPath(worldObject.mesh).c_str());
                    ImGui::SameLine();

                    selectAssetPopup("Mesh", worldObject.mesh, ImGui::Button("Change##Mesh"));

                    if (ImGui::TreeNode("Materials")) {
                        for (int i = 0; i < NUM_SUBMESH_MATS; i++) {
                            if (worldObject.presentMaterials[i]) {
                                ImGui::Text("Material %i: %s", i, AssetDB::idToPath(worldObject.materials[i]).c_str());
                            } else {
                                ImGui::Text("Material %i: not set", i);
                                worldObject.materials[i] = INVALID_ASSET;
                            }

                            ImGui::SameLine();

                            std::string idStr = "##" + std::to_string(i);

                            bool open = ImGui::Button(("Change" + idStr).c_str());
                            if (selectAssetPopup(("Material" + idStr).c_str(), worldObject.materials[i], open)) {
                                worldObject.materialIdx[i] = INVALID_ASSET;
                                worldObject.presentMaterials[i] = true;
                            }
                        }
                        ImGui::TreePop();
                    }
                }

                ImGui::Separator();
            }
        }

        void writeToFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file) override {
            auto& wObj = reg.get<WorldObject>(ent);
            WRITE_FIELD(file, wObj.staticFlags);
            for (int i = 0; i < NUM_SUBMESH_MATS; i++) {
                bool isPresent = wObj.presentMaterials[i];
                WRITE_FIELD(file, isPresent);

                if (isPresent) {
                    AssetID matId = wObj.materials[i];
                    WRITE_FIELD(file, matId);
                }
            }

            WRITE_FIELD(file, wObj.mesh);
            WRITE_FIELD(file, wObj.texScaleOffset);
            WRITE_FIELD(file, wObj.uvOverride);
        }

        void readFromFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file, int version) override {
            auto& wo = reg.emplace<WorldObject>(ent, 0, 0);

            if (version >= 3) {
                READ_FIELD(file, wo.staticFlags);
            }

            for (int i = 0; i < NUM_SUBMESH_MATS; i++) {
                bool isPresent;
                READ_FIELD(file, isPresent);
                wo.presentMaterials[i] = isPresent;

                AssetID mat;
                if (isPresent) {
                    READ_FIELD(file, mat);
                    wo.materials[i] = mat;
                    wo.materialIdx[i] = ~0u;
                }
            }

            READ_FIELD(file, wo.mesh);
            READ_FIELD(file, wo.texScaleOffset);

            if (version >= 4) {
                READ_FIELD(file, wo.uvOverride);
            }
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override {
            auto& wo = reg.get<WorldObject>(ent);

            uint32_t materialCount = wo.presentMaterials.count();
            nlohmann::json matArray;

            for (uint32_t i = 0; i < materialCount; i++) {
                matArray.push_back(AssetDB::idToPath(wo.materials[i]));
            }

            j = {
                { "mesh", AssetDB::idToPath(wo.mesh) },
                { "texScaleOffset", wo.texScaleOffset },
                { "uvOverride", wo.uvOverride },
                { "materials", matArray },
                { "staticFlags", wo.staticFlags }
            };
        }

        void fromJson(entt::entity ent, entt::registry& reg, const json& j) override {
            auto& wo = reg.emplace<WorldObject>(ent, 0, 0);
            std::string meshPath = j["mesh"];
            wo.mesh = AssetDB::pathToId(meshPath);
            wo.texScaleOffset = j["texScaleOffset"];
            wo.uvOverride = j["uvOverride"];
            wo.staticFlags = j["staticFlags"];

            uint32_t matIdx = 0;

            for (auto& v : j["materials"]) {
                wo.presentMaterials[matIdx] = true;
                if (v.is_number())
                    wo.materials[matIdx] = v;
                else {
                    std::string path = v;
                    wo.materials[matIdx] = AssetDB::pathToId(path);
                }
                matIdx++;
            }
        }
    };

    const std::unordered_map<LightType, const char*> lightTypeNames = {
            { LightType::Directional, "Directional" },
            { LightType::Point, "Point" },
            { LightType::Spot, "Spot" },
            { LightType::Sphere, "Sphere" },
            { LightType::Tube, "Tube" }
    };

    class WorldLightEditor : public BasicComponentUtil<WorldLight> {
    public:
        BASIC_CLONE(WorldLight);
        BASIC_CREATE(WorldLight);
        const char* getName() override { return "World Light"; }

        void edit(entt::entity ent, entt::registry& reg, Editor* ed) override {
            if (ImGui::CollapsingHeader(ICON_FA_LIGHTBULB u8" Light")) {
                if (ImGui::Button("Remove##WL")) {
                    reg.remove<WorldLight>(ent);
                } else {
                    auto& worldLight = reg.get<WorldLight>(ent);
                    ImGui::Checkbox("Enabled", &worldLight.enabled);
                    ImGui::ColorEdit3("Color", &worldLight.color.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
                    ImGui::DragFloat("Intensity", &worldLight.intensity);

                    if (ImGui::BeginCombo("Light Type", lightTypeNames.at(worldLight.type))) {
                        for (auto& p : lightTypeNames) {
                            bool isSelected = worldLight.type == p.first;
                            if (ImGui::Selectable(p.second, &isSelected)) {
                                worldLight.type = p.first;
                                if (p.first != LightType::Spot) {
                                    worldLight.enableShadows = false;
                                }
                            }

                            if (isSelected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }

                    if (worldLight.type == LightType::Spot) {
                        ImGui::DragFloat("Spot Cutoff", &worldLight.spotCutoff);
                        ImGui::Checkbox("Enable Shadows", &worldLight.enableShadows);
                    }

                    if (worldLight.type == LightType::Sphere) {
                        ImGui::DragFloat("Sphere Radius", &worldLight.spotCutoff);
                    }

                    if (worldLight.type == LightType::Tube) {
                        ImGui::DragFloat("Tube Length", &worldLight.tubeLength);
                        ImGui::DragFloat("Tube Radius", &worldLight.tubeRadius);
                    }
                }
            }
        }

        void writeToFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file) override {
            auto& wl = reg.get<WorldLight>(ent);
            WRITE_FIELD(file, wl.type);
            WRITE_FIELD(file, wl.color);
            WRITE_FIELD(file, wl.spotCutoff);
            WRITE_FIELD(file, wl.intensity);
            WRITE_FIELD(file, wl.tubeLength);
            WRITE_FIELD(file, wl.tubeRadius);
        }

        void readFromFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file, int version) override {
            auto& wl = reg.emplace<WorldLight>(ent);
            READ_FIELD(file, wl.type);
            READ_FIELD(file, wl.color);
            READ_FIELD(file, wl.spotCutoff);

            if (version >= 6) {
                READ_FIELD(file, wl.intensity);
                READ_FIELD(file, wl.tubeLength);
                READ_FIELD(file, wl.tubeRadius);
            }
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override {
            auto& wl = reg.get<WorldLight>(ent);

            j = {
                { "type", wl.type },
                { "color", wl.color },
                { "spotCutoff", wl.spotCutoff },
                { "intensity", wl.intensity },
                { "tubeLength", wl.tubeLength },
                { "tubeRadius", wl.tubeRadius },
                { "enableShadows", wl.enableShadows },
                { "enabled", wl.enabled }
            };
        }

        void fromJson(entt::entity ent, entt::registry& reg, const json& j) override {
            auto& wl = reg.emplace<WorldLight>(ent);

            wl.type = j["type"];
            wl.color = j["color"];
            wl.spotCutoff = j["spotCutoff"];
            wl.intensity = j["intensity"];
            wl.tubeLength = j["tubeLength"];
            wl.tubeRadius = j["tubeRadius"];
            wl.enableShadows = j.value("enableShadows", false);
            wl.enabled = j.value("enabled", true);
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


        if (doc.get_root().get_type() != sajson::TYPE_ARRAY) {
            logErr("Invalid collider JSON: Root object was not an array");
            return std::vector<PhysicsShape>{};
        }

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
                shape.box.halfExtents = glm::abs(glm::vec3{ shape.box.halfExtents.x, shape.box.halfExtents.z, shape.box.halfExtents.y });
            }

            shapes[i] = shape;
        }

        std::free(buf);

        return shapes;
    }

    template <typename T>
    void editPhysicsShapes(T& actor, Transform& actorTransform, worlds::Editor* ed) {
        static size_t currentShapeIdx = 0;
        static bool transformingShape = false;
        ImGui::Checkbox("Scale Shapes", &actor.scaleShapes);
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

        size_t i = 0;
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

            static Transform t;

            if (transformingShape && i == currentShapeIdx) {
                ed->overrideHandle(&t);
                if (ImGui::Button("Done")) {
                    transformingShape = false;
                    Transform shapeTransform = t.transformByInverse(actorTransform);
                    it->pos = shapeTransform.position;
                    it->rot = shapeTransform.rotation;
                }
            } else {
                if (ImGui::Button("Transform")) {
                    transformingShape = true;
                    currentShapeIdx = i;
                    t = Transform {it->pos, it->rot}.transformBy(actorTransform);
                }
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

            updatePhysicsShapes(newPhysActor, t.scale);
        }

        void edit(entt::entity ent, entt::registry& reg, Editor* ed) override {
            auto& pa = reg.get<PhysicsActor>(ent);
            if (ImGui::CollapsingHeader(ICON_FA_SHAPES u8" Physics Actor")) {
                if (ImGui::Button("Remove##PA")) {
                    reg.remove<PhysicsActor>(ent);
                } else {
                    if (ImGui::Button("Update Collisions")) {
                        updatePhysicsShapes(pa, reg.get<Transform>(ent).scale);
                    }

                    auto& t = reg.get<Transform>(ent);
                    editPhysicsShapes(pa, t, ed);
                }

                ImGui::Separator();
            }
        }

        void writeToFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file) override {
            PhysicsActor& pa = reg.get<PhysicsActor>(ent);
            PHYSFS_writeULE16(file, (uint16_t)pa.physicsShapes.size());

            for (size_t i = 0; i < pa.physicsShapes.size(); i++) {
                auto shape = pa.physicsShapes[i];
                WRITE_FIELD(file, shape.type);

                WRITE_FIELD(file, shape.pos);
                WRITE_FIELD(file, shape.rot);

                switch (shape.type) {
                case PhysicsShapeType::Sphere:
                    WRITE_FIELD(file, shape.sphere.radius);
                    break;
                case PhysicsShapeType::Box:
                    WRITE_FIELD(file, shape.box.halfExtents.x);
                    WRITE_FIELD(file, shape.box.halfExtents.y);
                    WRITE_FIELD(file, shape.box.halfExtents.z);
                    break;
                case PhysicsShapeType::Capsule:
                    WRITE_FIELD(file, shape.capsule.height);
                    WRITE_FIELD(file, shape.capsule.radius);
                    break;
                default:
                    logErr("invalid physics shape type??");
                    break;
                }
            }
        }

        void readFromFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file, int version) override {
            auto* pActor = g_physics->createRigidStatic(glm2px(reg.get<Transform>(ent)));
            g_scene->addActor(*pActor);

            PhysicsActor& pa = reg.emplace<PhysicsActor>(ent, pActor);

            uint16_t shapeCount;
            PHYSFS_readULE16(file, &shapeCount);

            pa.physicsShapes.resize(shapeCount);

            for (uint16_t i = 0; i < shapeCount; i++) {
                auto& shape = pa.physicsShapes[i];
                READ_FIELD(file, shape.type);

                READ_FIELD(file, shape.pos);
                READ_FIELD(file, shape.rot);

                switch (shape.type) {
                case PhysicsShapeType::Sphere:
                    READ_FIELD(file, shape.sphere.radius);
                    break;
                case PhysicsShapeType::Box:
                    READ_FIELD(file, shape.box.halfExtents.x);
                    READ_FIELD(file, shape.box.halfExtents.y);
                    READ_FIELD(file, shape.box.halfExtents.z);
                    break;
                case PhysicsShapeType::Capsule:
                    READ_FIELD(file, shape.capsule.height);
                    READ_FIELD(file, shape.capsule.radius);
                    break;
                default:
                    logErr("invalid physics shape type??");
                    break;
                }
            }

            auto& t = reg.get<Transform>(ent);

            updatePhysicsShapes(pa, t.scale);
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override {
            auto& pa = reg.get<PhysicsActor>(ent);
            json shapeArray;

            for (auto& shape : pa.physicsShapes) {
                json jShape = {
                    { "type", shape.type },
                    { "position", shape.pos },
                    { "rotation", shape.rot }
                };

                switch (shape.type) {
                    case PhysicsShapeType::Sphere:
                        jShape["radius"] = shape.sphere.radius;
                        break;
                    case PhysicsShapeType::Box:
                        jShape["halfExtents"] = shape.box.halfExtents;
                        break;
                    case PhysicsShapeType::Capsule:
                        jShape["height"] = shape.capsule.height;
                        jShape["radius"] = shape.capsule.radius;
                        break;
                    default:
                        assert(false && "invalid physics shape type");
                        break;
                }

                shapeArray.push_back(jShape);
            }

            j["shapes"] = shapeArray;
        }

        void fromJson(entt::entity ent, entt::registry& reg, const json& j) override {
            auto* pActor = g_physics->createRigidStatic(glm2px(reg.get<Transform>(ent)));
            g_scene->addActor(*pActor);

            PhysicsActor& pa = reg.emplace<PhysicsActor>(ent, pActor);

            for (auto& shape : j["shapes"]) {
                PhysicsShape ps;

                ps.type = shape["type"];

                switch (ps.type) {
                    case PhysicsShapeType::Sphere:
                        ps.sphere.radius = shape["radius"];
                        break;
                    case PhysicsShapeType::Box:
                        ps.box.halfExtents = shape["halfExtents"];
                        break;
                    case PhysicsShapeType::Capsule:
                        ps.capsule.height = shape["height"];
                        ps.capsule.radius = shape["radius"];
                        break;
                    default:
                        assert(false && "invalid physics shape type");
                        break;
                }

                ps.pos = shape["position"];
                ps.rot = shape["rotation"];

                pa.physicsShapes.push_back(ps);
            }

            auto& t = reg.get<Transform>(ent);

            updatePhysicsShapes(pa, t.scale);
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
            auto& oldDpa = reg.get<DynamicPhysicsActor>(from);

            physx::PxTransform pTf(glm2px(t.position), glm2px(t.rotation));
            auto* actor = g_physics->createRigidDynamic(pTf);

            auto& newPhysActor = reg.emplace<DynamicPhysicsActor>(to, actor);
            newPhysActor = oldDpa;
            newPhysActor.actor = actor;

            g_scene->addActor(*actor);

            updatePhysicsShapes(newPhysActor, t.scale);
        }

        void edit(entt::entity ent, entt::registry& reg, Editor* ed) override {
            auto& pa = reg.get<DynamicPhysicsActor>(ent);
            if (ImGui::CollapsingHeader(ICON_FA_SHAPES u8" Dynamic Physics Actor")) {
                if (ImGui::Button("Remove##DPA")) {
                    reg.remove<DynamicPhysicsActor>(ent);
                } else {
                    ImGui::DragFloat("Mass", &pa.mass);
                    ImGui::Checkbox("Enable Gravity", &pa.enableGravity);
                    ImGui::Checkbox("Enable CCD", &pa.enableCCD);
                    if (ImGui::Button("Update Collisions##DPA")) {
                        auto& t = reg.get<Transform>(ent);
                        updatePhysicsShapes(pa, t.scale);
                        updateMass(pa);
                    }

                    auto& t = reg.get<Transform>(ent);
                    editPhysicsShapes(pa, t, ed);
                }

                ImGui::Separator();
            }
        }

        void writeToFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file) override {
            DynamicPhysicsActor& pa = reg.get<DynamicPhysicsActor>(ent);
            float mass = ((physx::PxRigidBody*)pa.actor)->getMass();
            WRITE_FIELD(file, mass);
            PHYSFS_writeULE16(file, (uint16_t)pa.physicsShapes.size());

            for (size_t i = 0; i < pa.physicsShapes.size(); i++) {
                auto shape = pa.physicsShapes[i];
                WRITE_FIELD(file, shape.type);

                WRITE_FIELD(file, shape.pos);
                WRITE_FIELD(file, shape.rot);

                switch (shape.type) {
                case PhysicsShapeType::Sphere:
                    WRITE_FIELD(file, shape.sphere.radius);
                    break;
                case PhysicsShapeType::Box:
                    WRITE_FIELD(file, shape.box.halfExtents.x);
                    WRITE_FIELD(file, shape.box.halfExtents.y);
                    WRITE_FIELD(file, shape.box.halfExtents.z);
                    break;
                case PhysicsShapeType::Capsule:
                    WRITE_FIELD(file, shape.capsule.height);
                    WRITE_FIELD(file, shape.capsule.radius);
                    break;
                default:
                    logErr("invalid physics shape type??");
                    break;
                }
            }
        }

        void readFromFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file, int version) override {
            auto* pActor = g_physics->createRigidDynamic(glm2px(reg.get<Transform>(ent)));
            pActor->setSolverIterationCounts(12, 4);
            g_scene->addActor(*pActor);
            DynamicPhysicsActor& pa = reg.emplace<DynamicPhysicsActor>(ent, pActor);

            READ_FIELD(file, pa.mass);

            uint16_t shapeCount;
            PHYSFS_readULE16(file, &shapeCount);

            pa.physicsShapes.resize(shapeCount);

            for (uint16_t i = 0; i < shapeCount; i++) {
                auto& shape = pa.physicsShapes[i];
                READ_FIELD(file, shape.type);

                READ_FIELD(file, shape.pos);
                READ_FIELD(file, shape.rot);

                switch (shape.type) {
                case PhysicsShapeType::Sphere:
                    READ_FIELD(file, shape.sphere.radius);
                    break;
                case PhysicsShapeType::Box:
                    READ_FIELD(file, shape.box.halfExtents.x);
                    READ_FIELD(file, shape.box.halfExtents.y);
                    READ_FIELD(file, shape.box.halfExtents.z);
                    break;
                case PhysicsShapeType::Capsule:
                    READ_FIELD(file, shape.capsule.height);
                    READ_FIELD(file, shape.capsule.radius);
                    break;
                default:
                    logErr("invalid physics shape type??");
                    break;
                }
            }

            auto& t = reg.get<Transform>(ent);
            updatePhysicsShapes(pa, t.scale);
            updateMass(pa);
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override {
            auto& pa = reg.get<DynamicPhysicsActor>(ent);
            json shapeArray;

            for (auto& shape : pa.physicsShapes) {
                json jShape = {
                    { "type", shape.type },
                    { "position", shape.pos },
                    { "rotation", shape.rot }
                };

                switch (shape.type) {
                    case PhysicsShapeType::Sphere:
                        jShape["radius"] = shape.sphere.radius;
                        break;
                    case PhysicsShapeType::Box:
                        jShape["halfExtents"] = shape.box.halfExtents;
                        break;
                    case PhysicsShapeType::Capsule:
                        jShape["height"] = shape.capsule.height;
                        jShape["radius"] = shape.capsule.radius;
                        break;
                    default:
                        assert(false && "invalid physics shape type");
                        break;
                }

                shapeArray.push_back(jShape);
            }

            j["shapes"] = shapeArray;
            j["mass"] = pa.mass;
            j["enableCCD"] = pa.enableCCD;
            j["enableGravity"] = pa.enableGravity;
        }

        void fromJson(entt::entity ent, entt::registry& reg, const json& j) override {
            auto* pActor = g_physics->createRigidDynamic(glm2px(reg.get<Transform>(ent)));
            g_scene->addActor(*pActor);

            auto& pa = reg.emplace<DynamicPhysicsActor>(ent, pActor);

            for (auto& shape : j["shapes"]) {
                PhysicsShape ps;

                ps.type = shape["type"];

                switch (ps.type) {
                    case PhysicsShapeType::Sphere:
                        ps.sphere.radius = shape["radius"];
                        break;
                    case PhysicsShapeType::Box:
                        ps.box.halfExtents = shape["halfExtents"];
                        break;
                    case PhysicsShapeType::Capsule:
                        ps.capsule.height = shape["height"];
                        ps.capsule.radius = shape["radius"];
                        break;
                    default:
                        assert(false && "invalid physics shape type");
                        break;
                }
                ps.pos = shape["position"];
                ps.rot = shape["rotation"];

                pa.physicsShapes.push_back(ps);
            }

            auto& t = reg.get<Transform>(ent);

            updatePhysicsShapes(pa, t.scale);
            pa.mass = j["mass"];
            pa.enableCCD = j.value("enableCCD", false);
            pa.enableGravity = j.value("enableGravity", true);
            updateMass(pa);
        }
    };

    class NameComponentEditor : public BasicComponentUtil<NameComponent> {
    public:
        BASIC_CREATE(NameComponent);
        BASIC_CLONE(NameComponent);

        int getSortID() override {
            return -2;
        }

        const char* getName() override { return "Name Component"; }

        void edit(entt::entity ent, entt::registry& registry, Editor* ed) override {
            auto& nc = registry.get<NameComponent>(ent);

            ImGui::InputText("Name", &nc.name);
            ImGui::SameLine();
            if (ImGui::Button("Remove")) {
                registry.remove<NameComponent>(ent);
            }
            ImGui::Separator();
        }

        void writeToFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file) override {
            NameComponent& nc = reg.get<NameComponent>(ent);

            int nameLen = nc.name.length();
            WRITE_FIELD(file, nameLen);

            PHYSFS_writeBytes(file, nc.name.data(), nameLen);
        }

        void readFromFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file, int version) override {
            NameComponent& nc = reg.emplace<NameComponent>(ent);
            int nameLen;
            READ_FIELD(file, nameLen);

            nc.name.resize(nameLen);
            PHYSFS_readBytes(file, nc.name.data(), nameLen);
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override {
            auto& nc = reg.get<NameComponent>(ent);
            j = {
                { "name", nc.name }
            };
        }

        void fromJson(entt::entity ent, entt::registry& reg, const json& j) override {
            auto& nc = reg.emplace<NameComponent>(ent);
            nc.name = j["name"];
        }
    };

    class AudioSourceEditor : public BasicComponentUtil<AudioSource> {
    public:
        BASIC_CLONE(AudioSource);
        const char* getName() override { return "Audio Source"; }

        void create(entt::entity ent, entt::registry& reg) override {
            reg.emplace<AudioSource>(ent, AssetDB::pathToId("Audio/SFX/dlgsound.ogg"));
        }

        void edit(entt::entity ent, entt::registry& registry, Editor* ed) override {
            auto& as = registry.get<AudioSource>(ent);

            if (ImGui::CollapsingHeader(ICON_FAD_SPEAKER u8" Audio Source")) {
                ImGui::Checkbox("Loop", &as.loop);
                ImGui::Checkbox("Spatialise", &as.spatialise);
                ImGui::Checkbox("Play on scene open", &as.playOnSceneOpen);
                ImGui::Text("Current Asset Path: %s", AssetDB::idToPath(as.clipId).c_str());

                selectAssetPopup("Audio Source Path", as.clipId, ImGui::Button("Change"));

                if (ImGui::Button(ICON_FA_PLAY u8" Preview"))
                    AudioSystem::getInstance()->playOneShotClip(as.clipId, glm::vec3(0.0f));

                ImGui::Separator();
            }
        }

        void writeToFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file) override {
            auto& as = reg.get<AudioSource>(ent);

            WRITE_FIELD(file, as.clipId);
            WRITE_FIELD(file, as.channel);
            WRITE_FIELD(file, as.loop);
            WRITE_FIELD(file, as.playOnSceneOpen);
            WRITE_FIELD(file, as.spatialise);
            WRITE_FIELD(file, as.volume);
        }

        void readFromFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file, int version) override {
            AssetID clipId;
            READ_FIELD(file, clipId);
            auto& as = reg.emplace<AudioSource>(ent, clipId);
            READ_FIELD(file, as.channel);
            READ_FIELD(file, as.loop);
            READ_FIELD(file, as.playOnSceneOpen);
            READ_FIELD(file, as.spatialise);
            READ_FIELD(file, as.volume);
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override {
            auto& as = reg.get<AudioSource>(ent);

            j = {
                { "clipPath", AssetDB::idToPath(as.clipId) },
                { "channel", as.channel },
                { "loop", as.loop },
                { "playOnSceneOpen", as.playOnSceneOpen },
                { "spatialise", as.spatialise },
                { "volume", as.volume }
            };
        }

        void fromJson(entt::entity ent, entt::registry& reg, const json& j) override {
            std::string clipPath = j["clipPath"];
            AssetID id = AssetDB::pathToId(clipPath);
            auto& as = reg.emplace<AudioSource>(ent, id);

            as.channel = j["channel"];
            as.loop = j["loop"];
            as.playOnSceneOpen = j["playOnSceneOpen"];
            as.spatialise = j["spatialise"];
            as.volume = j["volume"];
        }
    };

    class WorldCubemapEditor : public BasicComponentUtil<WorldCubemap> {
    public:
        BASIC_CLONE(WorldCubemap);

        const char* getName() override { return "World Cubemap"; }

        void create(entt::entity ent, entt::registry& reg) override {
            auto& wc = reg.emplace<WorldCubemap>(ent);

            wc.cubemapId = AssetDB::pathToId("envmap_miramar/miramar.json");
            wc.extent = glm::vec3{ 1.0f };
        }

        void edit(entt::entity ent, entt::registry& reg, Editor* ed) override {
            auto& wc = reg.get<WorldCubemap>(ent);

            if (ImGui::CollapsingHeader(ICON_FA_CIRCLE u8" Cubemap")) {
                ImGui::DragFloat3("Extent", &wc.extent.x);
                ImGui::Checkbox("Parallax Correction", &wc.cubeParallax);
                ImGui::Text("Current Asset Path: %s", AssetDB::idToPath(wc.cubemapId).c_str());
                selectAssetPopup("Cubemap Path", wc.cubemapId, ImGui::Button("Change"));

                ImGui::Separator();
            }
        }

        void writeToFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file) override {
            WorldCubemap& wc = reg.get<WorldCubemap>(ent);

            WRITE_FIELD(file, wc.cubemapId);
            WRITE_FIELD(file, wc.extent);
            WRITE_FIELD(file, wc.cubeParallax);
        }

        void readFromFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file, int version) override {
            auto& wc = reg.emplace<WorldCubemap>(ent);

            READ_FIELD(file, wc.cubemapId);
            READ_FIELD(file, wc.extent);

            if (version >= 5) {
                READ_FIELD(file, wc.cubeParallax);
            }
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override {
            auto& wc = reg.get<WorldCubemap>(ent);

            j = {
                { "path", AssetDB::idToPath(wc.cubemapId) },
                { "useCubeParallax", wc.cubeParallax },
                { "extent", wc.extent }
            };
        }

        void fromJson(entt::entity ent, entt::registry& reg, const json& j) override {
            std::string path = j["path"];
            AssetID cubemapId = AssetDB::pathToId(path);
            auto& wc = reg.emplace<WorldCubemap>(ent);

            wc.cubemapId = cubemapId;
            wc.extent = j["extent"];
            wc.cubeParallax = j["useCubeParallax"];
        }
    };

    class ScriptComponentEditor : public BasicComponentUtil<ScriptComponent> {
    public:
        BASIC_CLONE(ScriptComponent);

        const char* getName() override { return "Script"; }

        void create(entt::entity ent, entt::registry& reg) override {
            reg.emplace<ScriptComponent>(ent, AssetDB::pathToId("Scripts/nothing.wren"));
        }

        void edit(entt::entity ent, entt::registry& reg, Editor* ed) override {
            auto& sc = reg.get<ScriptComponent>(ent);

            if (ImGui::CollapsingHeader(ICON_FA_SCROLL u8" Script")) {
                if (ImGui::Button("Remove")) {
                    reg.remove<ScriptComponent>(ent);
                } else {
                    ImGui::Text("Current Script Path: %s", AssetDB::idToPath(sc.script).c_str());
                    AssetID id = sc.script;
                    if (selectAssetPopup("Script Path", id, ImGui::Button("Change"))) {
                        reg.patch<ScriptComponent>(ent, [&](ScriptComponent& sc) {
                            sc.script = id;
                        });
                    }
                    ImGui::Separator();
                }
            }
        }

        void writeToFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file) override {
            auto& sc = reg.get<ScriptComponent>(ent);
            WRITE_FIELD(file, sc.script);
        }

        void readFromFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file, int version) override {
            AssetID aid;
            READ_FIELD(file, aid);
            reg.emplace<ScriptComponent>(ent, aid);
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override {
            auto& sc = reg.get<ScriptComponent>(ent);
            j = {
                { "path", AssetDB::idToPath(sc.script) }
            };
        }

        void fromJson(entt::entity ent, entt::registry& reg, const json& j) override {
            std::string path = j["path"];
            AssetID scriptID = AssetDB::pathToId(path);
            reg.emplace<ScriptComponent>(ent, scriptID);
        }
    };

    class ReverbProbeBoxEditor : public BasicComponentUtil<ReverbProbeBox> {
    public:
        BASIC_CLONE(ReverbProbeBox);

        const char* getName() override { return "Reverb Probe Box"; }

        BASIC_CREATE(ReverbProbeBox);

        void edit(entt::entity ent, entt::registry& reg, Editor* ed) override {
            auto& rpb = reg.get<ReverbProbeBox>(ent);

            if (ImGui::CollapsingHeader("Reverb Probe Box")) {
                ImGui::DragFloat3("Extents", glm::value_ptr(rpb.bounds));
                ImGui::Separator();
            }
        }

        void writeToFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file) override {
            auto& rbp = reg.get<ReverbProbeBox>(ent);
            WRITE_FIELD(file, rbp.bounds);
        }

        void readFromFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file, int version) override {
            auto& rbp = reg.emplace<ReverbProbeBox>(ent);
            READ_FIELD(file, rbp.bounds);
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override {
            auto& rbp = reg.get<ReverbProbeBox>(ent);

            j = {
                { "bounds", rbp.bounds }
            };
        }

        void fromJson(entt::entity ent, entt::registry& reg, const json& j) override {
            auto& rbp = reg.emplace<ReverbProbeBox>(ent);
            rbp.bounds = j["bounds"];
        }
    };

    class AudioTriggerEditor : public BasicComponentUtil<AudioTrigger> {
    public:
        BASIC_CLONE(AudioTrigger);
        BASIC_CREATE(AudioTrigger);

        const char* getName() override { return "Audio Trigger"; }

        void edit(entt::entity ent, entt::registry& reg, Editor* ed) override {
            auto& trigger = reg.get<AudioTrigger>(ent);

            if (ImGui::CollapsingHeader("Audio Trigger")) {
                if (ImGui::Button("Remove")) {
                    reg.remove<AudioTrigger>(ent);
                    return;
                }

                ImGui::Checkbox("Play Once", &trigger.playOnce);
                ImGui::Separator();
            }
        }

        // hasPlayed was incorrectly serialized here
        // to avoid breaking binary compatibility we'll just serialize dummy values
        // for now
        void writeToFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file) override {
            auto& at = reg.get<AudioTrigger>(ent);
            WRITE_FIELD(file, at.playOnce);

            bool hasPlayed = false;
            WRITE_FIELD(file, hasPlayed);
        }

        void readFromFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file, int version) override {
            auto& at = reg.emplace<AudioTrigger>(ent);
            READ_FIELD(file, at.playOnce);

            bool hasPlayed;
            READ_FIELD(file, hasPlayed);
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override {
            auto& at = reg.get<AudioTrigger>(ent);

            j = {
                { "playOnce", at.playOnce }
            };
        }

        void fromJson(entt::entity ent, entt::registry& reg, const json& j) override {
            auto& at = reg.emplace<AudioTrigger>(ent);

            at.playOnce = j["playOnce"];
        }
    };

    class ProxyAOEditor : public BasicComponentUtil<ProxyAOComponent> {
    public:
        BASIC_CLONE(ProxyAOComponent);
        BASIC_CREATE(ProxyAOComponent);

        const char* getName() override { return "AO Proxy"; }

        void edit(entt::entity ent, entt::registry& reg, Editor* ed) override {
            auto& pac = reg.get<ProxyAOComponent>(ent);

            if (ImGui::CollapsingHeader("AO Proxy")) {
                if (ImGui::Button("Remove")) {
                    reg.remove<ProxyAOComponent>(ent);
                    return;
                }
                ImGui::DragFloat3("bounds", &pac.bounds.x);
                ImGui::Separator();
            }
        }

        void writeToFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file) override {
            auto& pac = reg.get<ProxyAOComponent>(ent);
            WRITE_FIELD(file, pac.bounds);
        }

        void readFromFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file, int version) override {
            auto& pac = reg.emplace<ProxyAOComponent>(ent);
            READ_FIELD(file, pac.bounds);
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override {
            auto& pac = reg.get<ProxyAOComponent>(ent);

            j = {
                { "bounds", pac.bounds }
            };
        }

        void fromJson(entt::entity ent, entt::registry& reg, const json& j) override {
            auto& pac = reg.emplace<ProxyAOComponent>(ent);

            pac.bounds = j["bounds"];
        }
    };

    class WorldTextComponentEditor : public BasicComponentUtil<WorldTextComponent> {
    public:
        BASIC_CLONE(WorldTextComponent);
        BASIC_CREATE(WorldTextComponent);

        const char* getName() override { return "World Text Component"; }

        void edit(entt::entity ent, entt::registry& reg, Editor* ed) override {
            auto& wtc = reg.get<WorldTextComponent>(ent);

            if (ImGui::CollapsingHeader("World Text Component")) {
                if (ImGui::Button("Remove")) {
                    reg.remove<WorldTextComponent>(ent);
                    return;
                }

                if (wtc.font == INVALID_ASSET)
                    ImGui::Text("Using default font");
                else
                    ImGui::Text("Using font %s", AssetDB::idToPath(wtc.font).c_str());

                ImGui::SameLine();
                selectAssetPopup("Select SDF Font", wtc.font, ImGui::Button("Change##WTC"));
                ImGui::InputText("Text", &wtc.text);
                ImGui::DragFloat("Text Scale", &wtc.textScale);
                ImGui::Separator();
            }
        }

        void writeToFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file) override {
        }

        void readFromFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file, int version) override {
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override {
            auto& wtc = reg.get<WorldTextComponent>(ent);

            j = {
                { "text", wtc.text },
                { "textScale", wtc.textScale }
            };

            if (wtc.font != INVALID_ASSET)
                j["font"] = AssetDB::idToPath(wtc.font);
        }

        void fromJson(entt::entity ent, entt::registry& reg, const json& j) override {
            auto& wtc = reg.emplace<WorldTextComponent>(ent);

            wtc.text = j["text"];
            wtc.textScale = j["textScale"];

            if (j.contains("font"))
                wtc.font = AssetDB::pathToId(j["font"].get<std::string>());
        }
    };

    class PrefabInstanceEditor : public BasicComponentUtil<PrefabInstanceComponent> {
    public:
        BASIC_CLONE(PrefabInstanceComponent);
        BASIC_CREATE(PrefabInstanceComponent);

        const char* getName() override { return "Prefab Instance"; }

        void edit(entt::entity ent, entt::registry& reg, Editor* ed) override {
            auto& pic = reg.get<PrefabInstanceComponent>(ent);

            if (ImGui::CollapsingHeader("Prefab Instance")) {
                if (ImGui::Button("Remove")) {
                    reg.remove<PrefabInstanceComponent>(ent);
                    return;
                }

                ImGui::Text("Instance of %s", AssetDB::idToPath(pic.prefab).c_str());

                if (ImGui::Button("Apply Prefab")) {
                    PHYSFS_File* file = AssetDB::openAssetFileWrite(pic.prefab);
                    JsonSceneSerializer::saveEntity(file, reg, ent);
                    PHYSFS_close(file);
                }

                ImGui::Separator();
            }
        }

        void writeToFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file) override {
        }

        void readFromFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file, int version) override {
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override {
            j = nullptr;
        }

        void fromJson(entt::entity ent, entt::registry& reg, const json& j) override {
        }
    };

    TransformEditor transformEd;
    WorldObjectEditor worldObjEd;
    WorldLightEditor worldLightEd;
    PhysicsActorEditor paEd;
    DynamicPhysicsActorEditor dpaEd;
    NameComponentEditor ncEd;
    AudioSourceEditor asEd;
    WorldCubemapEditor wcEd;
    ScriptComponentEditor scEd;
    ReverbProbeBoxEditor rpbEd;
    AudioTriggerEditor atEd;
    ProxyAOEditor aoEd;
    WorldTextComponentEditor wtcEd;
    PrefabInstanceEditor pie;
}
