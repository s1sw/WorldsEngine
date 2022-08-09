#include "ComponentFuncs.hpp"
#include "ComponentEditorUtil.hpp"
#include <Audio/Audio.hpp>
#include <Core/Engine.hpp>
#include <Core/HierarchyUtil.hpp>
#include <Core/NameComponent.hpp>
#include <Core/Transform.hpp>
#include <Editor/Editor.hpp>
#include <Editor/GuiUtil.hpp>
#include <ImGui/imgui.h>
#include <Libs/IconsFontAwesome5.h>
#include <Libs/IconsFontaudio.h>
#include <Physics/D6Joint.hpp>
#include <Physics/Physics.hpp>
#include <Render/DebugLines.hpp>
#include <Render/Render.hpp>
#include <Scripting/ScriptComponent.hpp>
#include <Serialization/SceneSerialization.hpp>
#include <UI/WorldTextComponent.hpp>
#include <Util/CreateModelObject.hpp>
#include <Util/EnumUtil.hpp>
#include <Util/JsonUtil.hpp>
#include <entt/entt.hpp>
#include <foundation/PxTransform.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <nlohmann/json.hpp>
#include <robin_hood.h>

// Janky workaround to fix static constructors not being called
// (static constructors are only required to be called before the first function in the translation unit)
// (yay for typical c++ specification bullshittery)
#include "D6JointEditor.hpp"

using json = nlohmann::json;

namespace worlds
{
#define WRITE_FIELD(file, field) PHYSFS_writeBytes(file, &field, sizeof(field))
#define READ_FIELD(file, field) PHYSFS_readBytes(file, &field, sizeof(field))

    ComponentEditorLink* ComponentEditor::first = nullptr;

    ComponentEditor::ComponentEditor()
    {
        if (!first)
        {
            first = new ComponentEditorLink;
            first->next = nullptr;
        }
        else
        {
            ComponentEditorLink* next = first;
            first = new ComponentEditorLink;
            first->next = next;
        }

        first->editor = this;
    }

    class TransformEditor : public virtual BasicComponentUtil<Transform>
    {
      private:
        glm::vec3 getEulerAngles(glm::quat rotation)
        {
            float roll = atan2f(2.0f * rotation.y * rotation.w - 2.0f * rotation.x * rotation.z,
                                1.0f - 2.0f * rotation.y * rotation.y - 2.0f * rotation.z * rotation.z);
            float pitch = atan2f(2.0f * rotation.x * rotation.w - 2.0f * rotation.y * rotation.z,
                                 1.0f - 2.0f * rotation.x * rotation.x - 2.0f * rotation.z * rotation.z);
            float yaw = glm::asin(2.0f * rotation.x * rotation.y + 2.0f * rotation.z * rotation.w);

            return glm::vec3(pitch, roll, yaw);
        }

        glm::quat eulerQuat(glm::vec3 eulerAngles)
        {
            eulerAngles *= 0.5f;
            glm::vec3 c(cosf(eulerAngles.x), cosf(eulerAngles.y), cosf(eulerAngles.z));
            glm::vec3 s(sinf(eulerAngles.x), sinf(eulerAngles.y), sinf(eulerAngles.z));

            float w = c.x * c.y * c.z - s.x * s.y * s.z;
            float x = s.x * c.y * c.z + c.x * s.y * s.z;
            float y = c.x * s.y * c.z + s.x * c.y * s.z;
            float z = c.x * c.y * s.z - s.x * s.y * c.z;

            return glm::quat{w, x, y, z};
        }

        void showTransformControls(entt::registry& reg, Transform& selectedTransform, Editor* ed)
        {
            glm::vec3 pos = selectedTransform.position;
            if (ImGui::DragFloat3("Position", &pos.x))
            {
                ed->undo.pushState(reg);
                selectedTransform.position = pos;
            }

            glm::vec3 eulerRot = glm::degrees(getEulerAngles(selectedTransform.rotation));
            if (ImGui::DragFloat3("Rotation", glm::value_ptr(eulerRot)))
            {
                ed->undo.pushState(reg);
                selectedTransform.rotation = eulerQuat(glm::radians(eulerRot));
            }

            glm::vec3 scale = selectedTransform.scale;
            if (ImGui::DragFloat3("Scale", &scale.x) && !glm::any(glm::equal(scale, glm::vec3{0.0f})))
            {
                selectedTransform.scale = scale;
            }

            if (ImGui::Button("Snap to world grid"))
            {
                ed->undo.pushState(reg);
                selectedTransform.position = glm::round(selectedTransform.position);
                selectedTransform.scale = glm::round(selectedTransform.scale);
                eulerRot = glm::round(eulerRot / 15.0f) * 15.0f;
                selectedTransform.rotation = glm::radians(eulerRot);
            }

            ImGui::SameLine();

            if (ImGui::Button("Snap Rotation"))
            {
                ed->undo.pushState(reg);
                // eulerRot = glm::round(eulerRot / 15.0f) * 15.0f;
                glm::vec3 radEuler = glm::eulerAngles(selectedTransform.rotation);
                const float ROUND_TO = glm::half_pi<float>() * 0.25f;
                radEuler = glm::round(radEuler / ROUND_TO) * ROUND_TO;
                selectedTransform.rotation = radEuler;
            }
        }

      public:
        int getSortID() override
        {
            return -1;
        }

        const char* getName() override
        {
            return "Transform";
        }

        bool allowInspectorAdd() override
        {
            return false;
        }

        void edit(entt::entity ent, entt::registry& reg, Editor* ed) override
        {
            if (ImGui::CollapsingHeader(ICON_FA_ARROWS_ALT u8" Transform"))
            {
                auto& selectedTransform = reg.get<Transform>(ent);

                if (!reg.has<ChildComponent>(ent))
                {
                    showTransformControls(reg, selectedTransform, ed);
                }
                else
                {
                    auto& cc = reg.get<ChildComponent>(ent);
                    showTransformControls(reg, cc.offset, ed);

                    if (ImGui::TreeNode("World Space Transform"))
                    {
                        showTransformControls(reg, selectedTransform, ed);
                        ImGui::TreePop();
                    }
                }

                ImGui::Separator();
            }
        }

        void toJson(entt::entity ent, entt::registry& reg, nlohmann::json& j) override
        {
            const auto& t = reg.get<Transform>(ent);
            j = {{"position", t.position}, {"rotation", t.rotation}, {"scale", t.scale}};
        }

        void fromJson(entt::entity ent, entt::registry& reg, EntityIDMap&, const nlohmann::json& j) override
        {
            auto& t = reg.emplace<Transform>(ent);
            t.position = j["position"].get<glm::vec3>();
            t.rotation = j["rotation"].get<glm::quat>();
            t.scale = j["scale"].get<glm::vec3>();
        }
    };

    const robin_hood::unordered_flat_map<StaticFlags, const char*> flagNames = {
        {StaticFlags::Audio, "Audio"}, {StaticFlags::Rendering, "Rendering"}, {StaticFlags::Navigation, "Navigation"}};

    const char* uvOverrideNames[] = {"None", "XY", "XZ", "ZY", "Pick Best"};

    class WorldObjectEditor : public BasicComponentUtil<WorldObject>
    {
      public:
        const char* getName() override
        {
            return "World Object";
        }

        void create(entt::entity ent, entt::registry& reg) override
        {
            auto cubeId = AssetDB::pathToId("model.obj");
            auto matId = AssetDB::pathToId("Materials/DevTextures/dev_blue.json");
            reg.emplace<WorldObject>(ent, matId, cubeId);
        }

        void edit(entt::entity ent, entt::registry& reg, Editor* ed) override
        {
            if (ImGui::CollapsingHeader(ICON_FA_PENCIL_ALT u8" WorldObject"))
            {
                if (ImGui::Button("Remove##WO"))
                {
                    reg.remove<WorldObject>(ent);
                }
                else
                {
                    auto& worldObject = reg.get<WorldObject>(ent);
                    if (ImGui::TreeNode("Static Flags"))
                    {
                        for (int i = 1; i < 8; i <<= 1)
                        {
                            bool hasFlag = enumHasFlag(worldObject.staticFlags, (StaticFlags)i);
                            if (ImGui::Checkbox(flagNames.at((StaticFlags)i), &hasFlag))
                            {
                                int withoutI = (int)worldObject.staticFlags & (~i);
                                withoutI |= i * hasFlag;
                                worldObject.staticFlags = (StaticFlags)withoutI;
                            }
                        }
                        ImGui::TreePop();
                    }

                    if (ImGui::BeginCombo("UV Override", uvOverrideNames[(int)worldObject.uvOverride]))
                    {
                        int i = 0;
                        for (auto& p : uvOverrideNames)
                        {
                            bool isSelected = (int)worldObject.uvOverride == i;
                            if (ImGui::Selectable(p, &isSelected))
                            {
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

                    bool hitNotSet = false;
                    int notSetCount = 0;
                    if (ImGui::TreeNode("Materials"))
                    {
                        if (ImGui::Button("Reset"))
                        {
                            worldObject.materials[0] = AssetDB::pathToId("Materials/DevTextures/dev_blue.json");
                            worldObject.presentMaterials[0] = true;
                            for (int i = 1; i < NUM_SUBMESH_MATS; i++)
                            {
                                worldObject.materials[i] = INVALID_ASSET;
                                worldObject.presentMaterials[i] = false;
                            }
                        }
                        for (int i = 0; i < NUM_SUBMESH_MATS; i++)
                        {
                            bool showButton = false;

                            if (worldObject.presentMaterials[i])
                            {
                                if (hitNotSet)
                                {
                                    hitNotSet = false;
                                    ImGui::Text("%i unset materials", notSetCount);
                                    notSetCount = 0;
                                }

                                showButton = true;
                                ImGui::Text("Material %i: %s", i, AssetDB::idToPath(worldObject.materials[i]).c_str());
                            }
                            else
                            {
                                notSetCount++;
                                worldObject.materials[i] = INVALID_ASSET;

                                if (!hitNotSet)
                                {
                                    hitNotSet = true;
                                    ImGui::Text("Material %i: not set", i);
                                    showButton = true;
                                }
                            }

                            if (showButton)
                            {
                                ImGui::SameLine();

                                std::string idStr = "##" + std::to_string(i);

                                bool open = ImGui::Button(("Change" + idStr).c_str());
                                if (selectAssetPopup(("Material" + idStr).c_str(), worldObject.materials[i], open))
                                {
                                    worldObject.presentMaterials[i] = true;
                                }
                            }
                        }

                        if (hitNotSet)
                        {
                            hitNotSet = false;
                            ImGui::Text("%i unset materials", notSetCount);
                            notSetCount = 0;
                        }

                        ImGui::TreePop();
                    }
                }

                ImGui::Separator();
            }
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override
        {
            auto& wo = reg.get<WorldObject>(ent);

            uint32_t materialCount = wo.presentMaterials.count();
            nlohmann::json matArray;

            for (uint32_t i = 0; i < materialCount; i++)
            {
                matArray.push_back(AssetDB::idToPath(wo.materials[i]));
            }

            j = {{"mesh", AssetDB::idToPath(wo.mesh)},
                 {"texScaleOffset", wo.texScaleOffset},
                 {"uvOverride", wo.uvOverride},
                 {"materials", matArray},
                 {"staticFlags", wo.staticFlags}};
        }

        void fromJson(entt::entity ent, entt::registry& reg, EntityIDMap&, const json& j) override
        {
            auto& wo = reg.emplace<WorldObject>(ent, 0, 0);
            std::string meshPath = j["mesh"];
            wo.mesh = AssetDB::pathToId(meshPath);
            wo.texScaleOffset = j["texScaleOffset"];
            wo.uvOverride = j["uvOverride"];
            wo.staticFlags = j["staticFlags"];

            uint32_t matIdx = 0;

            for (auto& v : j["materials"])
            {
                wo.presentMaterials[matIdx] = true;
                if (v.is_number())
                    wo.materials[matIdx] = v;
                else
                {
                    std::string path = v;
                    wo.materials[matIdx] = AssetDB::pathToId(path);
                }
                matIdx++;
            }
        }
    };

    class SkinnedWorldObjectEditor : public BasicComponentUtil<SkinnedWorldObject>
    {
      public:
        const char* getName() override
        {
            return "Skinned World Object";
        }

        void create(entt::entity ent, entt::registry& reg) override
        {
            auto cubeId = AssetDB::pathToId("model.obj");
            auto matId = AssetDB::pathToId("Materials/DevTextures/dev_blue.json");
            reg.emplace<SkinnedWorldObject>(ent, matId, cubeId);
        }

        void edit(entt::entity ent, entt::registry& reg, Editor* ed) override
        {
            if (ImGui::CollapsingHeader(ICON_FA_PENCIL_ALT u8" SkinnedWorldObject"))
            {
                if (ImGui::Button("Remove##WO"))
                {
                    reg.remove<SkinnedWorldObject>(ent);
                }
                else
                {
                    auto& worldObject = reg.get<SkinnedWorldObject>(ent);
                    if (ImGui::TreeNode("Static Flags"))
                    {
                        for (int i = 1; i < 8; i <<= 1)
                        {
                            bool hasFlag = enumHasFlag(worldObject.staticFlags, (StaticFlags)i);
                            if (ImGui::Checkbox(flagNames.at((StaticFlags)i), &hasFlag))
                            {
                                int withoutI = (int)worldObject.staticFlags & (~i);
                                withoutI |= i * hasFlag;
                                worldObject.staticFlags = (StaticFlags)withoutI;
                            }
                        }
                        ImGui::TreePop();
                    }

                    if (ImGui::BeginCombo("UV Override", uvOverrideNames[(int)worldObject.uvOverride]))
                    {
                        int i = 0;
                        for (auto& p : uvOverrideNames)
                        {
                            bool isSelected = (int)worldObject.uvOverride == i;
                            if (ImGui::Selectable(p, &isSelected))
                            {
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

                    if (selectAssetPopup("Mesh", worldObject.mesh, ImGui::Button("Change##Mesh")))
                    {
                        worldObject.resetPose();
                    }

                    if (ImGui::TreeNode("Materials"))
                    {
                        for (int i = 0; i < NUM_SUBMESH_MATS; i++)
                        {
                            if (worldObject.presentMaterials[i])
                            {
                                ImGui::Text("Material %i: %s", i, AssetDB::idToPath(worldObject.materials[i]).c_str());
                            }
                            else
                            {
                                ImGui::Text("Material %i: not set", i);
                                worldObject.materials[i] = INVALID_ASSET;
                            }

                            ImGui::SameLine();

                            std::string idStr = "##" + std::to_string(i);

                            bool open = ImGui::Button(("Change" + idStr).c_str());
                            if (selectAssetPopup(("Material" + idStr).c_str(), worldObject.materials[i], open))
                            {
                                worldObject.presentMaterials[i] = true;
                            }
                        }
                        ImGui::TreePop();
                    }
                }

                ImGui::Separator();
            }
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override
        {
            auto& wo = reg.get<SkinnedWorldObject>(ent);

            uint32_t materialCount = wo.presentMaterials.count();
            nlohmann::json matArray;

            for (uint32_t i = 0; i < materialCount; i++)
            {
                matArray.push_back(AssetDB::idToPath(wo.materials[i]));
            }

            j = {{"mesh", AssetDB::idToPath(wo.mesh)},
                 {"texScaleOffset", wo.texScaleOffset},
                 {"uvOverride", wo.uvOverride},
                 {"materials", matArray},
                 {"staticFlags", wo.staticFlags}};
        }

        void fromJson(entt::entity ent, entt::registry& reg, EntityIDMap&, const json& j) override
        {
            std::string meshPath = j["mesh"];
            AssetID mesh = AssetDB::pathToId(meshPath);
            auto& wo = reg.emplace<SkinnedWorldObject>(ent, INVALID_ASSET, mesh);

            wo.texScaleOffset = j["texScaleOffset"];
            wo.uvOverride = j["uvOverride"];
            wo.staticFlags = j["staticFlags"];

            uint32_t matIdx = 0;

            for (auto& v : j["materials"])
            {
                wo.presentMaterials[matIdx] = true;
                if (v.is_number())
                    wo.materials[matIdx] = v;
                else
                {
                    std::string path = v;
                    wo.materials[matIdx] = AssetDB::pathToId(path);
                }
                matIdx++;
            }
        }
    };

    const std::unordered_map<LightType, const char*> lightTypeNames = {{LightType::Directional, "Directional"},
                                                                       {LightType::Point, "Point"},
                                                                       {LightType::Spot, "Spot"},
                                                                       {LightType::Sphere, "Sphere"},
                                                                       {LightType::Tube, "Tube"}};

    class WorldLightEditor : public BasicComponentUtil<WorldLight>
    {
      private:
        void showTypeSpecificControls(WorldLight& worldLight)
        {
            // Show options specific to certain types of light
            switch (worldLight.type)
            {
            case LightType::Spot: {
                float cutoff = glm::degrees(worldLight.spotCutoff);
                ImGui::DragFloat("Spot Cutoff", &cutoff, 1.0f, 0.0f, 90.0f);
                worldLight.spotCutoff = glm::radians(cutoff);

                float outerCutoff = glm::degrees(worldLight.spotOuterCutoff);
                ImGui::DragFloat("Spot Outer Cutoff", &outerCutoff, 1.0f, cutoff, 90.0f);
                worldLight.spotOuterCutoff = glm::radians(outerCutoff);

                ImGui::Checkbox("Enable Shadows", &worldLight.enableShadows);

                if (worldLight.enableShadows)
                {
                    ImGui::DragFloat("Near Plane", &worldLight.shadowNear, 0.1f, 0.001f, FLT_MAX);
                    ImGui::DragFloat("Far Plane", &worldLight.shadowFar, 1.0f, worldLight.shadowNear, FLT_MAX);
                }
            }
            break;
            case LightType::Sphere: {
                ImGui::DragFloat("Sphere Radius", &worldLight.spotCutoff, 1.0f, 0.0f, FLT_MAX);
            }
            break;
            case LightType::Tube: {
                ImGui::DragFloat("Tube Length", &worldLight.tubeLength, 0.1f, 0.0f, FLT_MAX);
                ImGui::DragFloat("Tube Radius", &worldLight.tubeRadius, 0.1f, 0.0f, FLT_MAX);
            }
            break;
            default:
                break;
            }
        }

      public:
        const char* getName() override
        {
            return "World Light";
        }

        void edit(entt::entity ent, entt::registry& reg, Editor* ed) override
        {
            if (ImGui::CollapsingHeader(ICON_FA_LIGHTBULB u8" Light"))
            {
                if (ImGui::Button("Remove##WL"))
                {
                    reg.remove<WorldLight>(ent);
                }
                else
                {
                    WorldLight& worldLight = reg.get<WorldLight>(ent);
                    Transform& transform = reg.get<Transform>(ent);

                    ImGui::Checkbox("Enabled", &worldLight.enabled);
                    ImGui::ColorEdit3("Color", &worldLight.color.x, ImGuiColorEditFlags_Float);
                    ImGui::DragFloat("Intensity", &worldLight.intensity, 0.1f, 0.000001f, FLT_MAX, "%.3f",
                                     ImGuiSliderFlags_AlwaysClamp);
                    tooltipHover("Controls brightness of the light in the scene.");

                    // Move the range sphere to center on the light
                    drawSphere(transform.position, transform.rotation, worldLight.maxDistance);

                    if (ImGui::BeginCombo("Light Type", lightTypeNames.at(worldLight.type)))
                    {
                        for (auto& p : lightTypeNames)
                        {
                            bool isSelected = worldLight.type == p.first;
                            if (ImGui::Selectable(p.second, &isSelected))
                            {
                                worldLight.type = p.first;
                                if (p.first != LightType::Spot)
                                {
                                    worldLight.enableShadows = false;
                                }
                            }

                            if (isSelected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }

                    // Show a rough guide for a radius - the point at which the light's
                    // intensity falls to less than 0.05
                    ImGui::Text("Recommended radius: %.3f", glm::sqrt(worldLight.intensity / 0.05f));
                    tooltipHover("This is the distance at which a light's influence falls below 5%.");

                    ImGui::DragFloat("Radius", &worldLight.maxDistance);
                    tooltipHover("Controls the maximum distance at which a light still affects objects.");

                    showTypeSpecificControls(worldLight);

                    if (worldLight.type == LightType::Spot)
                    {
                        glm::vec3 lightForward = transform.rotation * glm::vec3(0.0f, 0.0f, 1.0f);
                        float radiusAtCutoff = glm::tan(worldLight.spotCutoff) * worldLight.maxDistance * 0.5f;
                        drawCircle(transform.position + lightForward * worldLight.maxDistance * 0.5f, radiusAtCutoff,
                                   transform.rotation * glm::angleAxis(glm::half_pi<float>(), glm::vec3(1.f, 0.f, 0.f)),
                                   glm::vec4(1.0f));
                    }
                }
            }
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override
        {
            auto& wl = reg.get<WorldLight>(ent);

            j = {{"type", wl.type},
                 {"color", wl.color},
                 {"spotCutoff", wl.spotCutoff},
                 {"spotOuterCutoff", wl.spotOuterCutoff},
                 {"intensity", wl.intensity},
                 {"tubeLength", wl.tubeLength},
                 {"tubeRadius", wl.tubeRadius},
                 {"enableShadows", wl.enableShadows},
                 {"enabled", wl.enabled},
                 {"maxDistance", wl.maxDistance},
                 {"shadowNear", wl.shadowNear},
                 {"shadowFar", wl.shadowFar}};
        }

        void fromJson(entt::entity ent, entt::registry& reg, EntityIDMap&, const json& j) override
        {
            auto& wl = reg.emplace<WorldLight>(ent);

            wl.type = j["type"];
            wl.color = j["color"];
            wl.spotCutoff = j["spotCutoff"];
            wl.spotOuterCutoff = j.value("spotOuterCutoff", glm::pi<float>() * 0.6f);
            wl.intensity = j["intensity"];
            wl.tubeLength = j["tubeLength"];
            wl.tubeRadius = j["tubeRadius"];
            wl.enableShadows = j.value("enableShadows", false);
            wl.enabled = j.value("enabled", true);

            if (!j.contains("maxDistance") && j.contains("distanceCutoff"))
            {
                wl.maxDistance = 1.0f / (sqrtf(j["distanceCutoff"]));
            }
            else
            {
                wl.maxDistance = j.value("maxDistance", wl.maxDistance);
            }

            wl.shadowNear = j.value("shadowNear", wl.shadowNear);
            wl.shadowFar = j.value("shadowFar", wl.shadowFar);
        }
    };

    const char* shapeTypeNames[(int)PhysicsShapeType::Count] = {"Sphere", "Box", "Capsule", "Mesh", "Convex Mesh"};

    PhysicsShapeType valToShapeType(std::string str)
    {
        if (str == "CUBE")
            return PhysicsShapeType::Box;
        else
            return PhysicsShapeType::Sphere;
    }

    std::vector<PhysicsShape> loadColliderJson(const char* path)
    {
        auto* file = PHYSFS_openRead(path);
        auto len = PHYSFS_fileLength(file);

        std::string str;
        str.resize(len);

        PHYSFS_readBytes(file, str.data(), len);
        PHYSFS_close(file);

        json j = json::parse(str);

        std::vector<PhysicsShape> shapes;
        shapes.reserve(j.size());

        for (const auto& el : j)
        {
            PhysicsShape shape;
            // const auto& typeval = el.get_value_of_key(sajson::string{ "type", 4 });
            shape.type = valToShapeType(el["type"]);

            shape.pos = el["position"];
            glm::vec3 eulerAngles = el["rotation"];
            shape.pos = glm::vec3{shape.pos.x, shape.pos.z, -shape.pos.y};
            eulerAngles = glm::vec3{eulerAngles.x, eulerAngles.z, -eulerAngles.y};
            shape.rot = glm::quat{eulerAngles};

            if (shape.type == PhysicsShapeType::Box)
            {
                shape.box.halfExtents = el["scale"];
                shape.box.halfExtents =
                    glm::abs(glm::vec3{shape.box.halfExtents.x, shape.box.halfExtents.z, shape.box.halfExtents.y});
            }

            shapes.push_back(shape);
        }

        return shapes;
    }

    glm::vec4 physShapeColor{0.f, 1.f, 0.f, 1.f};
    // Draws a box shape using lines.
    void drawPhysicsBox(const Transform& actorTransform, const PhysicsShape& ps)
    {
        Transform shapeTransform{ps.pos * actorTransform.scale, ps.rot};
        shapeTransform = shapeTransform.transformBy(actorTransform);
        drawBox(shapeTransform.position, shapeTransform.rotation, ps.box.halfExtents * actorTransform.scale,
                physShapeColor);
    }

    void drawPhysicsSphere(const Transform& actorTransform, const PhysicsShape& ps)
    {
        Transform shapeTransform{ps.pos * actorTransform.scale, ps.rot};
        shapeTransform = shapeTransform.transformBy(actorTransform);
        drawSphere(shapeTransform.position, shapeTransform.rotation, ps.sphere.radius, physShapeColor);
    }

    void drawPhysicsCapsule(const Transform& actorTransform, const PhysicsShape& ps)
    {
        Transform shapeTransform{ps.pos * actorTransform.scale, ps.rot};
        shapeTransform = shapeTransform.transformBy(actorTransform);
        drawCapsule(shapeTransform.position,
                    shapeTransform.rotation * glm::quat{glm::vec3{0.0f, 0.0f, glm::half_pi<float>()}},
                    ps.capsule.height * 0.5f, ps.capsule.radius, physShapeColor);
    }

    void drawPhysicsMesh(const Transform& actorTransform, const PhysicsShape& ps)
    {
        AssetID meshId = ps.type == PhysicsShapeType::ConvexMesh ? ps.convexMesh.mesh : ps.mesh.mesh;
        const LoadedMesh& lm = MeshManager::loadOrGet(meshId);

        for (size_t i = 0; i < lm.indices.size(); i += 3)
        {
            glm::vec3 p0 =
                actorTransform.transformPoint(lm.vertices[lm.indices[i + 0]].position * actorTransform.scale);
            glm::vec3 p1 =
                actorTransform.transformPoint(lm.vertices[lm.indices[i + 1]].position * actorTransform.scale);
            glm::vec3 p2 =
                actorTransform.transformPoint(lm.vertices[lm.indices[i + 2]].position * actorTransform.scale);

            drawLine(p0, p1, physShapeColor);
            drawLine(p1, p2, physShapeColor);
            drawLine(p0, p2, physShapeColor);
        }
    }

    // Draws the given shape using lines.
    void drawPhysicsShape(const Transform& actorTransform, const PhysicsShape& ps)
    {
        switch (ps.type)
        {
        case PhysicsShapeType::Box:
            drawPhysicsBox(actorTransform, ps);
            break;
        case PhysicsShapeType::Sphere:
            drawPhysicsSphere(actorTransform, ps);
            break;
        case PhysicsShapeType::ConvexMesh:
        case PhysicsShapeType::Mesh:
            drawPhysicsMesh(actorTransform, ps);
            break;
        case PhysicsShapeType::Capsule:
            drawPhysicsCapsule(actorTransform, ps);
            break;
        default:
            break;
        }
    }

    template <typename T> void editPhysicsShapes(T& actor, Transform& actorTransform, worlds::Editor* ed)
    {
        static size_t currentShapeIdx = 0;
        static bool transformingShape = false;
        ImGui::Checkbox("Scale Shapes", &actor.scaleShapes);
        if (ImGui::Button("Load Collider JSON"))
        {
            ImGui::OpenPopup("Collider JSON");
        }

        const char* extension = ".json";
        openFileModalOffset(
            "Collider JSON", [&](const char* p) { actor.physicsShapes = loadColliderJson(p); }, "SourceData/",
            &extension, 1, "ColliderJsons");

        ImGui::Text("Shapes: %zu", actor.physicsShapes.size());

        ImGui::SameLine();

        if (ImGui::Button("Add"))
        {
            actor.physicsShapes.push_back(PhysicsShape::boxShape(glm::vec3(0.5f)));
        }

        std::vector<PhysicsShape>::iterator eraseIter;
        bool erase = false;

        size_t i = 0;
        for (auto it = actor.physicsShapes.begin(); it != actor.physicsShapes.end(); it++)
        {
            ImGui::PushID(i);
            if (ImGui::BeginCombo("Collider Type", shapeTypeNames[(int)it->type]))
            {
                for (int iType = 0; iType < (int)PhysicsShapeType::Count; iType++)
                {
                    auto type = (PhysicsShapeType)iType;
                    bool isSelected = it->type == type;
                    if (ImGui::Selectable(shapeTypeNames[iType], &isSelected))
                    {
                        it->type = type;

                        if (type == PhysicsShapeType::Mesh)
                        {
                            it->mesh.mesh = ~0u;
                        }

                        if (type == PhysicsShapeType::ConvexMesh)
                        {
                            it->convexMesh.mesh = ~0u;
                        }
                    }

                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            drawPhysicsShape(actorTransform, *it);

            if (ImGui::Button("Remove Shape"))
            {
                eraseIter = it;
                erase = true;
            }

            static Transform t;

            if (transformingShape && i == currentShapeIdx)
            {
                ed->overrideHandle(&t);
                if (ImGui::Button("Done"))
                {
                    transformingShape = false;
                    Transform shapeTransform = t.transformByInverse(actorTransform);
                    it->pos = shapeTransform.position;
                    it->rot = shapeTransform.rotation;
                }
            }
            else
            {
                if (ImGui::Button("Transform"))
                {
                    transformingShape = true;
                    currentShapeIdx = i;
                    t = Transform{it->pos, it->rot}.transformBy(actorTransform);
                }
            }

            ImGui::DragFloat3("Position", &it->pos.x);

            switch (it->type)
            {
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
            case PhysicsShapeType::Mesh:
                if (it->mesh.mesh == ~0u)
                    ImGui::Text("No mesh set");
                else
                    ImGui::Text("%s", AssetDB::idToPath(it->mesh.mesh).c_str());
                selectAssetPopup("Mesh", it->mesh.mesh, ImGui::Button("Change"));
                break;
            case PhysicsShapeType::ConvexMesh:
                if (it->convexMesh.mesh == ~0u)
                    ImGui::Text("No mesh set");
                else
                    ImGui::Text("%s", AssetDB::idToPath(it->convexMesh.mesh).c_str());
                selectAssetPopup("Mesh", it->convexMesh.mesh, ImGui::Button("Change"));
                break;
            default:
                break;
            }
            ImGui::PopID();
            i++;
        }

        if (erase)
            actor.physicsShapes.erase(eraseIter);
    }

    class PhysicsActorEditor : public BasicComponentUtil<PhysicsActor>
    {
      public:
        const char* getName() override
        {
            return "Physics Actor";
        }

        void create(entt::entity ent, entt::registry& reg) override
        {
            auto& t = reg.get<Transform>(ent);

            physx::PxTransform pTf(glm2px(t.position), glm2px(t.rotation));
            auto* actor = interfaces->physics->physics()->createRigidStatic(pTf);
            reg.emplace<PhysicsActor>(ent, actor);
            interfaces->physics->scene()->addActor(*actor);
        }

        void clone(entt::entity from, entt::entity to, entt::registry& reg) override
        {
            auto& t = reg.get<Transform>(from);

            physx::PxTransform pTf(glm2px(t.position), glm2px(t.rotation));
            auto* actor = interfaces->physics->physics()->createRigidStatic(pTf);

            auto& newPhysActor = reg.emplace<PhysicsActor>(to, actor);
            newPhysActor.physicsShapes = reg.get<PhysicsActor>(from).physicsShapes;

            interfaces->physics->scene()->addActor(*actor);

            interfaces->physics->updatePhysicsShapes(newPhysActor, t.scale);
        }

        void edit(entt::entity ent, entt::registry& reg, Editor* ed) override
        {
            auto& pa = reg.get<PhysicsActor>(ent);
            if (ImGui::CollapsingHeader(ICON_FA_SHAPES u8" Physics Actor"))
            {
                if (ImGui::Button("Remove##PA"))
                {
                    reg.remove<PhysicsActor>(ent);
                }
                else
                {
                    if (ImGui::Button("Update Collisions"))
                    {
                        interfaces->physics->updatePhysicsShapes(pa, reg.get<Transform>(ent).scale);
                    }

                    auto& t = reg.get<Transform>(ent);
                    editPhysicsShapes(pa, t, ed);
                }

                ImGui::Separator();
            }
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override
        {
            auto& pa = reg.get<PhysicsActor>(ent);
            json shapeArray;

            for (auto& shape : pa.physicsShapes)
            {
                json jShape = {{"type", shape.type}, {"position", shape.pos}, {"rotation", shape.rot}};

                switch (shape.type)
                {
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
                case PhysicsShapeType::Mesh:
                    jShape["mesh"] = AssetDB::idToPath(shape.mesh.mesh);
                    break;
                default:
                    assert(false && "invalid physics shape type");
                    break;
                }

                shapeArray.push_back(jShape);
            }

            j["shapes"] = shapeArray;
            j["layer"] = pa.layer;
        }

        void fromJson(entt::entity ent, entt::registry& reg, EntityIDMap&, const json& j) override
        {
            auto* pActor = interfaces->physics->physics()->createRigidStatic(glm2px(reg.get<Transform>(ent)));
            interfaces->physics->scene()->addActor(*pActor);

            PhysicsActor& pa = reg.emplace<PhysicsActor>(ent, pActor);

            for (auto& shape : j["shapes"])
            {
                PhysicsShape ps;

                ps.type = shape["type"];

                switch (ps.type)
                {
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
                case PhysicsShapeType::Mesh:
                    ps.mesh.mesh = AssetDB::pathToId(shape["mesh"].get<std::string>());
                    break;
                default:
                    assert(false && "invalid physics shape type");
                    break;
                }

                ps.pos = shape["position"];
                ps.rot = shape["rotation"];

                pa.physicsShapes.push_back(ps);
            }

            pa.layer = j.value("layer", 1);

            if (pa.layer == 0)
                pa.layer = 1;

            auto& t = reg.get<Transform>(ent);

            interfaces->physics->updatePhysicsShapes(pa, t.scale);
        }
    };

    class RigidBodyEditor : public BasicComponentUtil<RigidBody>
    {
      public:
        int getSortID() override
        {
            return 1;
        }

        const char* getName() override
        {
            return "Dynamic Physics Actor";
        }

        void create(entt::entity ent, entt::registry& reg) override
        {
            auto& t = reg.get<Transform>(ent);

            physx::PxTransform pTf(glm2px(t.position), glm2px(t.rotation));
            auto* actor = interfaces->physics->physics()->createRigidDynamic(pTf);
            actor->setSolverIterationCounts(32, 6);
            actor->setMaxDepenetrationVelocity(10.0f);
            actor->setMaxAngularVelocity(1000.0f);
            reg.emplace<RigidBody>(ent, actor);
            interfaces->physics->scene()->addActor(*actor);
        }

        void clone(entt::entity from, entt::entity to, entt::registry& reg) override
        {
            auto& t = reg.get<Transform>(from);
            auto& oldDpa = reg.get<RigidBody>(from);

            physx::PxTransform pTf(glm2px(t.position), glm2px(t.rotation));
            auto* actor = interfaces->physics->physics()->createRigidDynamic(pTf);

            auto& newPhysActor = reg.emplace<RigidBody>(to, actor);
            newPhysActor.mass = oldDpa.mass;
            newPhysActor.actor = actor;
            newPhysActor.physicsShapes = oldDpa.physicsShapes;
            newPhysActor.layer = oldDpa.layer;
            newPhysActor.setLockFlags(oldDpa.lockFlags());
            newPhysActor.enableGravity = oldDpa.enableGravity;
            newPhysActor.enableCCD = oldDpa.enableCCD;
            newPhysActor.layer = oldDpa.layer;

            interfaces->physics->scene()->addActor(*actor);

            interfaces->physics->updatePhysicsShapes(newPhysActor, t.scale);
        }

        void edit(entt::entity ent, entt::registry& reg, Editor* ed) override
        {
            auto& pa = reg.get<RigidBody>(ent);
            if (ImGui::CollapsingHeader(ICON_FA_SHAPES u8" Dynamic Physics Actor"))
            {
                if (ImGui::Button("Remove##DPA"))
                {
                    reg.remove<RigidBody>(ent);
                }
                else
                {
                    if (ImGui::TreeNode("Lock Flags"))
                    {
                        uint32_t flags = (uint32_t)pa.lockFlags();

                        ImGui::CheckboxFlags("Lock X", &flags, (uint32_t)DPALockFlags::LinearX);
                        ImGui::CheckboxFlags("Lock Y", &flags, (uint32_t)DPALockFlags::LinearY);
                        ImGui::CheckboxFlags("Lock Z", &flags, (uint32_t)DPALockFlags::LinearZ);

                        ImGui::CheckboxFlags("Lock Angular X", &flags, (uint32_t)DPALockFlags::AngularX);
                        ImGui::CheckboxFlags("Lock Angular Y", &flags, (uint32_t)DPALockFlags::AngularY);
                        ImGui::CheckboxFlags("Lock Angular Z", &flags, (uint32_t)DPALockFlags::AngularZ);

                        pa.setLockFlags((DPALockFlags)flags);
                        ImGui::TreePop();
                    }

                    ImGui::DragScalar("Layer", ImGuiDataType_U32, &pa.layer);
                    ImGui::DragFloat("Mass", &pa.mass);
                    bool enabled = pa.enabled();

                    if (ImGui::Checkbox("Enabled", &enabled))
                    {
                        pa.setEnabled(enabled);
                    }

                    ImGui::Checkbox("Enable Gravity", &pa.enableGravity);
                    ImGui::Checkbox("Enable CCD", &pa.enableCCD);
                    if (ImGui::Button("Update Collisions##DPA"))
                    {
                        auto& t = reg.get<Transform>(ent);
                        interfaces->physics->updatePhysicsShapes(pa, t.scale);
                        updateMass(pa);
                    }

                    auto& t = reg.get<Transform>(ent);
                    editPhysicsShapes(pa, t, ed);
                }

                ImGui::Separator();
            }
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override
        {
            auto& pa = reg.get<RigidBody>(ent);
            json shapeArray;

            for (auto& shape : pa.physicsShapes)
            {
                json jShape = {{"type", shape.type}, {"position", shape.pos}, {"rotation", shape.rot}};

                switch (shape.type)
                {
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
                case PhysicsShapeType::ConvexMesh:
                    jShape["mesh"] = AssetDB::idToPath(shape.convexMesh.mesh);
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
            j["lockFlags"] = (uint32_t)pa.lockFlags();
            j["layer"] = pa.layer;
            j["scaleShapes"] = pa.scaleShapes;
            if (!pa.enabled())
                j["enabled"] = false;
        }

        void fromJson(entt::entity ent, entt::registry& reg, EntityIDMap&, const json& j) override
        {
            auto* pActor = interfaces->physics->physics()->createRigidDynamic(glm2px(reg.get<Transform>(ent)));
            pActor->setSolverIterationCounts(32, 6);
            pActor->setSleepThreshold(0.005f);
            pActor->setMaxDepenetrationVelocity(10.0f);
            pActor->setMaxAngularVelocity(1000.0f);
            interfaces->physics->scene()->addActor(*pActor);

            auto& pa = reg.emplace<RigidBody>(ent, pActor);
            pa.scaleShapes = j.value("scaleShapes", true);

            for (auto& shape : j["shapes"])
            {
                PhysicsShape ps;

                ps.type = shape["type"];

                switch (ps.type)
                {
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
                case PhysicsShapeType::ConvexMesh:
                    ps.convexMesh.mesh = AssetDB::pathToId(shape["mesh"].get<std::string>());
                    break;
                default:
                    assert(false && "invalid physics shape type");
                    break;
                }
                ps.pos = shape["position"];
                ps.rot = shape["rotation"];

                pa.physicsShapes.push_back(ps);
            }

            pa.layer = j.value("layer", 1);

            if (pa.layer == 0)
                pa.layer = 1;

            auto& t = reg.get<Transform>(ent);

            interfaces->physics->updatePhysicsShapes(pa, t.scale);
            pa.mass = j["mass"];
            pa.enableCCD = j.value("enableCCD", false);
            pa.enableGravity = j.value("enableGravity", true);
            updateMass(pa);

            if (j.contains("lockFlags"))
                pa.setLockFlags((DPALockFlags)j["lockFlags"].get<uint32_t>());

            if (!j.value("enabled", true))
                pa.setEnabled(false);
        }
    };

    class NameComponentEditor : public BasicComponentUtil<NameComponent>
    {
      public:
        int getSortID() override
        {
            return -2;
        }

        const char* getName() override
        {
            return "Name Component";
        }

        void edit(entt::entity ent, entt::registry& registry, Editor* ed) override
        {
            auto& nc = registry.get<NameComponent>(ent);

            ImGui::InputText("Name", &nc.name);
            ImGui::SameLine();
            if (ImGui::Button("Remove##Name"))
            {
                registry.remove<NameComponent>(ent);
            }
            ImGui::Separator();
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override
        {
            auto& nc = reg.get<NameComponent>(ent);
            j = {{"name", nc.name}};
        }

        void fromJson(entt::entity ent, entt::registry& reg, EntityIDMap&, const json& j) override
        {
            auto& nc = reg.emplace<NameComponent>(ent);
            nc.name = j["name"];
        }
    };

    class AudioSourceEditor : public BasicComponentUtil<OldAudioSource>
    {
      public:
        const char* getName() override
        {
            return "Audio Source";
        }

        void create(entt::entity ent, entt::registry& reg) override
        {
            reg.emplace<OldAudioSource>(ent, AssetDB::pathToId("Audio/SFX/dlgsound.ogg"));
        }

        void edit(entt::entity ent, entt::registry& registry, Editor* ed) override
        {
            auto& as = registry.get<OldAudioSource>(ent);

            if (ImGui::CollapsingHeader(ICON_FAD_SPEAKER u8" Audio Source"))
            {

                if (ImGui::Button("Remove"))
                {
                    registry.remove<OldAudioSource>(ent);
                    return;
                }
                ImGui::Checkbox("Loop", &as.loop);
                ImGui::Checkbox("Spatialise", &as.spatialise);
                ImGui::Checkbox("Play on scene open", &as.playOnSceneOpen);
                ImGui::DragFloat("Volume", &as.volume);
                ImGui::Text("Current Asset Path: %s", AssetDB::idToPath(as.clipId).c_str());

                selectAssetPopup("Audio Source Path", as.clipId, ImGui::Button("Change"));

                if (ImGui::Button(ICON_FA_PLAY u8" Preview"))
                    AudioSystem::getInstance()->playOneShotClip(as.clipId, glm::vec3(0.0f));

                ImGui::Separator();
            }
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override
        {
            auto& as = reg.get<OldAudioSource>(ent);

            j = {{"clipPath", AssetDB::idToPath(as.clipId)}, {"channel", as.channel},       {"loop", as.loop},
                 {"playOnSceneOpen", as.playOnSceneOpen},    {"spatialise", as.spatialise}, {"volume", as.volume}};
        }

        void fromJson(entt::entity ent, entt::registry& reg, EntityIDMap&, const json& j) override
        {
            std::string clipPath = j["clipPath"];
            AssetID id = AssetDB::pathToId(clipPath);
            auto& as = reg.emplace<OldAudioSource>(ent, id);

            as.channel = j["channel"];
            as.loop = j["loop"];
            as.playOnSceneOpen = j["playOnSceneOpen"];
            as.spatialise = j["spatialise"];
            as.volume = j["volume"];
        }
    };

    class FMODAudioSourceEditor : public BasicComponentUtil<AudioSource>
    {
      public:
        const char* getName() override
        {
            return "FMOD Audio Source";
        }

        void create(entt::entity ent, entt::registry& reg) override
        {
            reg.emplace<AudioSource>(ent);
        }

        void edit(entt::entity ent, entt::registry& registry, Editor* ed) override
        {
            auto& as = registry.get<AudioSource>(ent);

            if (ImGui::CollapsingHeader(ICON_FAD_SPEAKER u8" Audio Source"))
            {
                static bool editingEventPath = false;
                static std::string currentEventPath;

                if (!editingEventPath)
                {
                    ImGui::Text("Current event path: %s", as.eventPath().data());

                    if (ImGui::Button("Change"))
                    {
                        editingEventPath = true;
                        currentEventPath.assign(as.eventPath());
                    }
                }
                else
                {
                    if (ImGui::InputText("Event Path", &currentEventPath, ImGuiInputTextFlags_EnterReturnsTrue))
                    {
                        editingEventPath = false;
                        as.changeEventPath(currentEventPath);
                    }
                }

                ImGui::Checkbox("Play On Scene Start", &as.playOnSceneStart);

                if (as.eventInstance != nullptr)
                {
                    if (as.playbackState() != FMOD_STUDIO_PLAYBACK_PLAYING)
                    {
                        if (ImGui::Button(ICON_FA_PLAY u8" Preview"))
                            as.eventInstance->start();
                    }
                    else
                    {
                        if (ImGui::Button(ICON_FA_STOP u8" Stop"))
                            as.eventInstance->stop(FMOD_STUDIO_STOP_ALLOWFADEOUT);
                    }
                }

                ImGui::Separator();
            }
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override
        {
            auto& as = reg.get<AudioSource>(ent);
            std::string evtPath;
            evtPath.assign(as.eventPath());

            j = {{"eventPath", evtPath}, {"playOnSceneStart", as.playOnSceneStart}};
        }

        void fromJson(entt::entity ent, entt::registry& reg, EntityIDMap&, const json& j) override
        {
            auto& as = reg.emplace<AudioSource>(ent);
            as.changeEventPath(j["eventPath"].get<std::string>());
            as.playOnSceneStart = j["playOnSceneStart"];
        }
    };

    class WorldCubemapEditor : public BasicComponentUtil<WorldCubemap>
    {
      public:
        const char* getName() override
        {
            return "World Cubemap";
        }

        void create(entt::entity ent, entt::registry& reg) override
        {
            auto& wc = reg.emplace<WorldCubemap>(ent);

            wc.cubemapId = AssetDB::pathToId("envmap_miramar/miramar.json");
            wc.extent = glm::vec3{1.0f};
        }

        void edit(entt::entity ent, entt::registry& reg, Editor* ed) override
        {
            auto& wc = reg.get<WorldCubemap>(ent);
            wc.cubemapId = AssetDB::pathToId("LevelData/Cubemaps/" + reg.ctx<SceneInfo>().name + "/" +
                                             reg.get<NameComponent>(ent).name + ".json");

            if (ImGui::CollapsingHeader(ICON_FA_CIRCLE u8" Cubemap"))
            {
                ImGui::DragFloat3("Extent", &wc.extent.x);
                ImGui::DragFloat3("Capture Offset", glm::value_ptr(wc.captureOffset));
                ImGui::InputInt("Resolution", &wc.resolution);
                tooltipHover("Powers of two are highly recommended for this setting.");
                ImGui::Checkbox("Parallax Correction", &wc.cubeParallax);
                ImGui::InputInt("Priority", &wc.priority);
                tooltipHover("Cubemaps with a higher priority value will be preferred over cubemaps with a lower "
                             "priority value.");

                Transform boundsTransform{};
                boundsTransform.position = reg.get<Transform>(ent).position;
                boundsTransform.scale = wc.extent;
                drawBox(boundsTransform.position, glm::quat{1.0f, 0.0f, 0.0f, 0.0f}, wc.extent);
                drawSphere(boundsTransform.position + wc.captureOffset, glm::quat{1.0f, 0.0f, 0.0f, 0.0f}, 0.25f,
                           glm::vec4(1.0f));

                ImGui::Separator();
            }
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override
        {
            auto& wc = reg.get<WorldCubemap>(ent);

            j = {{"useCubeParallax", wc.cubeParallax},
                 {"extent", wc.extent},
                 {"priority", wc.priority},
                 {"resolution", wc.resolution},
                 {"captureOffset", wc.captureOffset}};
        }

        void fromJson(entt::entity ent, entt::registry& reg, EntityIDMap&, const json& j) override
        {
            auto& wc = reg.emplace<WorldCubemap>(ent);

            wc.cubemapId = AssetDB::pathToId("LevelData/Cubemaps/" + reg.ctx<SceneInfo>().name + "/" +
                                             reg.get<NameComponent>(ent).name + ".json");
            wc.extent = j["extent"];
            wc.cubeParallax = j["useCubeParallax"];
            wc.priority = j.value("priority", 0);
            wc.resolution = j.value("resolution", 128);
            wc.captureOffset = j.value("captureOffset", glm::vec3{0.0f});
        }
    };

    class ScriptComponentEditor : public BasicComponentUtil<ScriptComponent>
    {
      public:
        const char* getName() override
        {
            return "Script";
        }

        void create(entt::entity ent, entt::registry& reg) override
        {
            reg.emplace<ScriptComponent>(ent, AssetDB::pathToId("Scripts/nothing.wren"));
        }

        void edit(entt::entity ent, entt::registry& reg, Editor* ed) override
        {
            auto& sc = reg.get<ScriptComponent>(ent);

            if (ImGui::CollapsingHeader(ICON_FA_SCROLL u8" Script"))
            {
                if (ImGui::Button("Remove##Script"))
                {
                    reg.remove<ScriptComponent>(ent);
                }
                else
                {
                    ImGui::Text("Current Script Path: %s", AssetDB::idToPath(sc.script).c_str());
                    AssetID id = sc.script;
                    if (selectAssetPopup("Script Path", id, ImGui::Button("Change")))
                    {
                        reg.patch<ScriptComponent>(ent, [&](ScriptComponent& sc) { sc.script = id; });
                    }
                    ImGui::Separator();
                }
            }
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override
        {
            auto& sc = reg.get<ScriptComponent>(ent);
            j = {{"path", AssetDB::idToPath(sc.script)}};
        }

        void fromJson(entt::entity ent, entt::registry& reg, EntityIDMap&, const json& j) override
        {
            std::string path = j["path"];
            AssetID scriptID = AssetDB::pathToId(path);
            reg.emplace<ScriptComponent>(ent, scriptID);
        }
    };

    class ReverbProbeBoxEditor : public BasicComponentUtil<ReverbProbeBox>
    {
      public:
        const char* getName() override
        {
            return "Reverb Probe Box";
        }

        void edit(entt::entity ent, entt::registry& reg, Editor* ed) override
        {
            if (ImGui::CollapsingHeader("Reverb Probe Box"))
            {
                ImGui::Separator();
            }
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override
        {
            j = {};
        }

        void fromJson(entt::entity ent, entt::registry& reg, EntityIDMap&, const json& j) override
        {
            reg.emplace<ReverbProbeBox>(ent);
        }
    };

    class AudioTriggerEditor : public BasicComponentUtil<AudioTrigger>
    {
      public:
        const char* getName() override
        {
            return "Audio Trigger";
        }

        void edit(entt::entity ent, entt::registry& reg, Editor* ed) override
        {
            auto& trigger = reg.get<AudioTrigger>(ent);

            if (ImGui::CollapsingHeader("Audio Trigger"))
            {
                if (ImGui::Button("Remove##AudioTrigger"))
                {
                    reg.remove<AudioTrigger>(ent);
                    return;
                }

                ImGui::Checkbox("Play Once", &trigger.playOnce);
                ImGui::Separator();
            }
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override
        {
            auto& at = reg.get<AudioTrigger>(ent);

            j = {{"playOnce", at.playOnce}};
        }

        void fromJson(entt::entity ent, entt::registry& reg, EntityIDMap&, const json& j) override
        {
            auto& at = reg.emplace<AudioTrigger>(ent);

            at.playOnce = j["playOnce"];
        }
    };

    class ProxyAOEditor : public BasicComponentUtil<ProxyAOComponent>
    {
      public:
        const char* getName() override
        {
            return "AO Proxy";
        }

        void edit(entt::entity ent, entt::registry& reg, Editor* ed) override
        {
            auto& pac = reg.get<ProxyAOComponent>(ent);

            if (ImGui::CollapsingHeader("AO Proxy"))
            {
                if (ImGui::Button("Remove##AOProxy"))
                {
                    reg.remove<ProxyAOComponent>(ent);
                    return;
                }
                ImGui::DragFloat3("bounds", &pac.bounds.x);
                ImGui::Separator();
            }
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override
        {
            auto& pac = reg.get<ProxyAOComponent>(ent);

            j = {{"bounds", pac.bounds}};
        }

        void fromJson(entt::entity ent, entt::registry& reg, EntityIDMap&, const json& j) override
        {
            auto& pac = reg.emplace<ProxyAOComponent>(ent);

            pac.bounds = j["bounds"];
        }
    };

    class WorldTextComponentEditor : public BasicComponentUtil<WorldTextComponent>
    {
      public:
        const char* getName() override
        {
            return "World Text Component";
        }

        void edit(entt::entity ent, entt::registry& reg, Editor* ed) override
        {
            auto& wtc = reg.get<WorldTextComponent>(ent);

            if (ImGui::CollapsingHeader("World Text Component"))
            {
                if (ImGui::Button("Remove##WorldText"))
                {
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

        void toJson(entt::entity ent, entt::registry& reg, json& j) override
        {
            auto& wtc = reg.get<WorldTextComponent>(ent);

            j = {{"text", wtc.text}, {"textScale", wtc.textScale}};

            if (wtc.font != INVALID_ASSET)
                j["font"] = AssetDB::idToPath(wtc.font);
        }

        void fromJson(entt::entity ent, entt::registry& reg, EntityIDMap&, const json& j) override
        {
            auto& wtc = reg.emplace<WorldTextComponent>(ent);

            wtc.text = j["text"];
            wtc.textScale = j["textScale"];

            if (j.contains("font"))
                wtc.font = AssetDB::pathToId(j["font"].get<std::string>());
        }
    };

    class PrefabInstanceEditor : public BasicComponentUtil<PrefabInstanceComponent>
    {
      public:
        const char* getName() override
        {
            return "Prefab Instance";
        }

        void edit(entt::entity ent, entt::registry& reg, Editor* ed) override
        {
            auto& pic = reg.get<PrefabInstanceComponent>(ent);

            if (ImGui::CollapsingHeader("Prefab Instance"))
            {
                if (ImGui::Button("Remove##PrefabInstance"))
                {
                    reg.remove<PrefabInstanceComponent>(ent);
                    return;
                }

                ImGui::Text("Instance of %s", AssetDB::idToPath(pic.prefab).c_str());

                if (ImGui::Button("Apply Prefab"))
                {
                    std::string path = AssetDB::idToPath(pic.prefab);
                    if (path.find("SourceData") == std::string::npos)
                    {
                        path = "SourceData/" + path;
                    }
                    PHYSFS_File* file = PHYSFS_openWrite(path.c_str());
                    JsonSceneSerializer::saveEntity(file, reg, ent);
                    PHYSFS_close(file);
                }

                ImGui::Separator();
            }
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override
        {
            j = nullptr;
        }

        void fromJson(entt::entity ent, entt::registry& reg, EntityIDMap&, const json& j) override
        {
        }
    };

    class SphereAOProxyEditor : public BasicComponentUtil<SphereAOProxy>
    {
      public:
        const char* getName() override
        {
            return "Sphere AO Proxy";
        }

        void edit(entt::entity entity, entt::registry& reg, Editor* ed) override
        {
            auto& proxy = reg.get<SphereAOProxy>(entity);

            if (ImGui::CollapsingHeader("Sphere AO Proxy"))
            {
                if (ImGui::Button("Remove##SphereAO"))
                {
                    reg.remove<SphereAOProxy>(entity);
                    return;
                }

                ImGui::DragFloat("Radius", &proxy.radius);
            }
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override
        {
            auto& proxy = reg.get<SphereAOProxy>(ent);

            j = {{"radius", proxy.radius}};
        }

        void fromJson(entt::entity ent, entt::registry& reg, EntityIDMap&, const json& j) override
        {
            auto& proxy = reg.emplace<SphereAOProxy>(ent);

            proxy.radius = j["radius"];
        }
    };

    class EditorLabelEditor : public BasicComponentUtil<EditorLabel>
    {
      public:
        const char* getName() override
        {
            return "Editor Label";
        }

        void edit(entt::entity entity, entt::registry& reg, Editor* ed) override
        {
            auto& label = reg.get<EditorLabel>(entity);

            if (ImGui::CollapsingHeader("Editor Label"))
            {
                if (ImGui::Button("Remove##EditorLabel"))
                {
                    reg.remove<EditorLabel>(entity);
                    return;
                }

                ImGui::InputText("Label", &label.label);
            }
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override
        {
            auto& label = reg.get<EditorLabel>(ent);

            j = {{"label", label.label}};
        }

        void fromJson(entt::entity ent, entt::registry& reg, EntityIDMap&, const json& j) override
        {
            auto& label = reg.emplace<EditorLabel>(ent);

            label.label = j["label"];
        }
    };

    class AudioListenerOverrideEditor : public BasicComponentUtil<AudioListenerOverride>
    {
      public:
        const char* getName() override
        {
            return "AudioListenerOverride";
        }

        void edit(entt::entity entity, entt::registry& reg, Editor* ed) override
        {
            if (ImGui::CollapsingHeader("AudioListenerOverride"))
            {
                if (ImGui::Button("Remove##AudioListenerOverride"))
                {
                    reg.remove<AudioListenerOverride>(entity);
                    return;
                }
            }
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override
        {
            j = {};
        }

        void fromJson(entt::entity ent, entt::registry& reg, EntityIDMap&, const json& j) override
        {
            reg.emplace<AudioListenerOverride>(ent);
        }
    };

    class ChildComponentEditor : public BasicComponentUtil<ChildComponent>
    {
      public:
        const char* getName() override
        {
            return "ChildComponent";
        }

        void create(entt::entity ent, entt::registry& reg) override
        {
            reg.emplace<ChildComponent>(ent);
        }

        bool allowInspectorAdd() override
        {
            return false;
        }

        void edit(entt::entity entity, entt::registry& reg, Editor* ed) override
        {
            static bool changingTarget = false;

            if (ImGui::CollapsingHeader("ChildComponent"))
            {
                if (ImGui::Button("Remove##ChildComponent"))
                {
                    reg.remove<ChildComponent>(entity);
                    return;
                }

                ChildComponent& cc = reg.get<ChildComponent>(entity);

                if (!changingTarget)
                {
                    if (ImGui::Button("Change"))
                    {
                        changingTarget = true;
                    }
                }

                if (changingTarget)
                {
                    if (ed->entityEyedropper(cc.parent))
                    {
                        changingTarget = false;
                    }
                }
            }
            else
            {
                changingTarget = false;
            }
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override
        {
            ChildComponent& c = reg.get<ChildComponent>(ent);

            j = {{"parent", (uint32_t)c.parent},
                 {"position", c.offset.position},
                 {"rotation", c.offset.rotation},
                 {"scale", c.offset.scale}};
        }

        void fromJson(entt::entity ent, entt::registry& reg, EntityIDMap& idMap, const json& j) override
        {
            if (!j.is_object())
                return;
            HierarchyUtil::setEntityParent(reg, ent, (entt::entity)idMap[j["parent"]]);

            auto& c = reg.get<ChildComponent>(ent);
            c.offset.position = j.value("position", glm::vec3(0.0f));
            c.offset.rotation = j.value("rotation", glm::quat{1.0f, 0.0f, 0.0f, 0.0f});
            c.offset.scale = j.value("scale", glm::vec3(1.0f));
        }
    };

    TransformEditor transformEd;
    WorldObjectEditor worldObjEd;
    SkinnedWorldObjectEditor skWorldObjEd;
    WorldLightEditor worldLightEd;
    PhysicsActorEditor paEd;
    RigidBodyEditor dpaEd;
    NameComponentEditor ncEd;
    AudioSourceEditor asEd;
    WorldCubemapEditor wcEd;
    ScriptComponentEditor scEd;
    ReverbProbeBoxEditor rpbEd;
    AudioTriggerEditor atEd;
    ProxyAOEditor aoEd;
    WorldTextComponentEditor wtcEd;
    PrefabInstanceEditor pie;
    SphereAOProxyEditor saope;
    EditorLabelEditor ele;
    FMODAudioSourceEditor fase;
    AudioListenerOverrideEditor alo;
    ChildComponentEditor ced;
}
