#include <entt/entt.hpp>
#include <ComponentMeta/ComponentEditorUtil.hpp>
#include <Core/Log.hpp>
#include "Core/Engine.hpp"
#include "Editor/Editor.hpp"
#include "PlayerStartPoint.hpp"
#include "LocospherePlayerSystem.hpp"
#include <ImGui/imgui.h>
#include "GripPoint.hpp"
#include <glm/gtc/type_ptr.hpp>
#include "PhysicsSoundComponent.hpp"
#include <Editor/GuiUtil.hpp>
#include <nlohmann/json.hpp>
#include <Util/JsonUtil.hpp>
#include "RPGStats.hpp"
#include "ContactDamageDealer.hpp"
#include "Grabbable.hpp"
#include "Gun.hpp"
#include "DamagingProjectile.hpp"
#include "Stabby.hpp"

using json = nlohmann::json;

namespace lg {
#define BASIC_WRITE_TO_FILE(type) \
    void writeToFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file) override { \
        auto& c = reg.get<type>(ent); PHYSFS_writeBytes(file, &c, sizeof(c)); \
    }

#define BASIC_READ_FROM_FILE(type) \
    void readFromFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file, int version) override { \
        auto& c = reg.emplace<type>(ent); PHYSFS_readBytes(file, &c, sizeof(c)); \
    }

    class PlayerStartPointEditor : public worlds::BasicComponentUtil<PlayerStartPoint> {
    public:
        BASIC_CREATE(PlayerStartPoint);
        BASIC_CLONE(PlayerStartPoint);

        const char* getName() override { return "Player Start Point"; }

        void edit(entt::entity ent, entt::registry& reg, worlds::Editor* ed) override {
            if (ImGui::CollapsingHeader("Player Start Point")) {
                if (ImGui::Button("Remove##PSP")) {
                    reg.remove<PlayerStartPoint>(ent);
                }

                ImGui::Separator();
            }
        }

        void writeToFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file) override {
            auto& psp = reg.get<PlayerStartPoint>(ent);

            PHYSFS_writeBytes(file, &psp, sizeof(psp));
        }

        void readFromFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file, int version) override {
            auto& psp = reg.emplace<PlayerStartPoint>(ent);
            PHYSFS_readBytes(file, &psp, sizeof(psp));
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override {
            auto& psp = reg.get<PlayerStartPoint>(ent);

            j = {
                { "alwaysUse", psp.alwaysUse }
            };
        }

        void fromJson(entt::entity ent, entt::registry& reg, const json& j) override {
            auto& psp = reg.emplace<PlayerStartPoint>(ent);

            psp.alwaysUse = j["alwaysUse"];
        }
    };

    class PlayerRigEditor : public worlds::BasicComponentUtil<PlayerRig> {
    public:
        BASIC_CREATE(PlayerRig);
        BASIC_CLONE(PlayerRig);

        const char* getName() override { return "Player Rig"; }

        void edit(entt::entity ent, entt::registry& reg, worlds::Editor* ed) override {
        }

        void writeToFile(entt::entity, entt::registry&, PHYSFS_File*) override {}
        void readFromFile(entt::entity, entt::registry&, PHYSFS_File*, int) override {}
        void toJson(entt::entity ent, entt::registry& reg, json& j) override {}
        void fromJson(entt::entity ent, entt::registry& reg, const json& j) override {}
    };

#define WRITE_FIELD(file, field) PHYSFS_writeBytes(file, &field, sizeof(field))
#define READ_FIELD(file, field) PHYSFS_readBytes(file, &field, sizeof(field))

    class GripPointEditor : public worlds::BasicComponentUtil<GripPoint> {
    public:
        BASIC_CREATE(GripPoint);
        BASIC_CLONE(GripPoint);

        const char* getName() override { return "Grip Point"; }

        void edit(entt::entity ent, entt::registry& reg, worlds::Editor* ed) override {
            if (ImGui::CollapsingHeader("Grip Point")) {
                if (ImGui::Button("Remove##Grip")) {
                    reg.remove<GripPoint>(ent);
                    return;
                }

                auto& gp = reg.get<GripPoint>(ent);
                ImGui::DragFloat3("Pos Offset", glm::value_ptr(gp.offset));
                ImGui::DragFloat4("Rot Offset", glm::value_ptr(gp.rotOffset));
            }
        }

        void writeToFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file) override {
            auto& gp = reg.get<GripPoint>(ent);
            WRITE_FIELD(file, gp.offset);
            WRITE_FIELD(file, gp.rotOffset);
        }

        void readFromFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file, int version) override {
            auto& gp = reg.emplace<GripPoint>(ent);

            READ_FIELD(file, gp.offset);
            READ_FIELD(file, gp.rotOffset);
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override {
            auto& gp = reg.get<GripPoint>(ent);

            j = {
                { "offset", gp.offset },
                { "rotOffset", gp.rotOffset }
            };
        }

        void fromJson(entt::entity ent, entt::registry& reg, const json& j) override {
            auto& gp = reg.emplace<GripPoint>(ent);

            gp.offset = j["offset"];
            gp.rotOffset = j["rotOffset"];
        }
    };

    class PhysicsSoundComponentEditor : public worlds::BasicComponentUtil<PhysicsSoundComponent> {
    public:
        BASIC_CREATE(PhysicsSoundComponent);
        BASIC_CLONE(PhysicsSoundComponent);

        const char* getName() override { return "Physics Sound"; }

        void edit(entt::entity ent, entt::registry& reg, worlds::Editor* ed) override {
            auto& psc = reg.get<PhysicsSoundComponent>(ent);
            if (ImGui::CollapsingHeader(" Phys Sound")) {
                if (psc.soundId != ~0u) {
                    ImGui::Text("Current Asset Path: %s", worlds::AssetDB::idToPath(psc.soundId).c_str());
                } else {
                    ImGui::Text("Sound not selected");
                }

                worlds::selectAssetPopup("Physics Audio Source Path", psc.soundId, ImGui::Button("Change"));

                ImGui::Separator();
            }
        }

        void writeToFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file) override {
            auto& psc = reg.get<PhysicsSoundComponent>(ent);
            WRITE_FIELD(file, psc.soundId);
        }

        void readFromFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file, int version) override {
            auto& psc = reg.emplace<PhysicsSoundComponent>(ent);
            READ_FIELD(file, psc.soundId);
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override {
            auto& psc = reg.get<PhysicsSoundComponent>(ent);
            j = {
                { "soundPath", worlds::AssetDB::idToPath(psc.soundId) }
            };
        }

        void fromJson(entt::entity ent, entt::registry& reg, const json& j) override {
            auto& psc = reg.emplace<PhysicsSoundComponent>(ent);

            std::string soundPath = j["soundPath"];
            psc.soundId = worlds::AssetDB::pathToId(soundPath);
        }
    };

    class RPGStatsComponentEditor : public worlds::BasicComponentUtil<RPGStats> {
    public:
        BASIC_CREATE(RPGStats);
        BASIC_CLONE(RPGStats);

        const char* getName() override { return "RPG Stats"; }

        void edit(entt::entity ent, entt::registry& reg, worlds::Editor* ed) override {
            auto& rpgStats = reg.get<RPGStats>(ent);

            if (ImGui::CollapsingHeader(" RPG Stats")) {
                ImGui::DragScalar("maxHP", ImGuiDataType_U64, &rpgStats.maxHP, 1.0f);
                ImGui::DragScalar("currentHP", ImGuiDataType_U64, &rpgStats.currentHP, 1.0f);
                ImGui::DragScalar("level", ImGuiDataType_U64, &rpgStats.level, 1.0f);
                ImGui::DragScalar("totalExperience", ImGuiDataType_U64, &rpgStats.totalExperience, 1.0f);
                ImGui::DragScalar("strength", ImGuiDataType_U8, &rpgStats.strength, 1.0f);
            }
        }

        void writeToFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file) override {
            auto& rpgStats = reg.get<RPGStats>(ent);
            WRITE_FIELD(file, rpgStats);
        }

        void readFromFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file, int version) override {
            auto& rpgStats = reg.emplace<RPGStats>(ent);
            READ_FIELD(file, rpgStats);
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override {
            auto& rpgStats = reg.get<RPGStats>(ent);

            j = {
                { "maxHP", rpgStats.maxHP },
                { "currentHP", rpgStats.currentHP },
                { "level", rpgStats.level },
                { "totalExperience", rpgStats.totalExperience },
                { "strength", rpgStats.strength }
            };
        }

        void fromJson(entt::entity ent, entt::registry& reg, const json& j) override {
            auto& rpgStats = reg.emplace<RPGStats>(ent);

            rpgStats.maxHP = j["maxHP"];
            rpgStats.currentHP = j["currentHP"];
            rpgStats.level = j["level"];
            rpgStats.totalExperience = j["totalExperience"];
            rpgStats.strength = j["strength"];
        }
    };

    class ContactDamageDealerComponentEditor : public worlds::BasicComponentUtil<ContactDamageDealer> {
    public:
        BASIC_CREATE(ContactDamageDealer);
        BASIC_CLONE(ContactDamageDealer);
        BASIC_WRITE_TO_FILE(ContactDamageDealer);
        BASIC_READ_FROM_FILE(ContactDamageDealer);

        const char* getName() override { return "Contact Damage Dealer"; }

        void edit(entt::entity ent, entt::registry& reg, worlds::Editor* ed) override {
            auto& cdd = reg.get<ContactDamageDealer>(ent);

            if (ImGui::CollapsingHeader("Contact Damage Dealer")) {
                ImGui::DragScalar("Damage", ImGuiDataType_U8, &cdd.damage, 1.0f);
                ImGui::DragFloat("Min Velocity", &cdd.minVelocity);
                ImGui::DragFloat("Max Velocity", &cdd.maxVelocity);
            }
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override {
            auto& cdd = reg.get<ContactDamageDealer>(ent);

            j = {
                { "damage", cdd.damage },
                { "minVelocity", cdd.minVelocity },
                { "maxVelocity", cdd.maxVelocity }
            };
        }

        void fromJson(entt::entity ent, entt::registry& reg, const json& j) override {
            auto& cdd = reg.emplace<ContactDamageDealer>(ent);

            cdd.damage = j["damage"];
            cdd.minVelocity = j["minVelocity"];
            cdd.maxVelocity = j["maxVelocity"];
        }
    };

    const char* gripTypeNames[] = {
        "Manual",
        "Box",
        "Cylinder"
    };

    const char* gripHandNames[] = {
        "Left",
        "Right",
        "Both"
    };

    template <typename T,
              typename TIter = decltype(std::begin(std::declval<T>())),
              typename = decltype(std::end(std::declval<T>()))>
    constexpr auto enumerate(T && iterable) {
        struct iterator {
            size_t i;
            TIter iter;
            bool operator != (const iterator & other) const { return iter != other.iter; }
            void operator ++ () { ++i; ++iter; }
            auto operator * () const { return std::tie(i, *iter); }
        };
        struct iterable_wrapper {
            T iterable;
            auto begin() { return iterator{ 0, std::begin(iterable) }; }
            auto end() { return iterator{ 0, std::end(iterable) }; }
        };
        return iterable_wrapper{ std::forward<T>(iterable) };
    }

    entt::entity createHandPreviewEnt(bool leftHand, entt::registry& registry) {
        auto matId = worlds::AssetDB::pathToId("Materials/VRHands/placeholder.json");
        auto lHandModel = worlds::AssetDB::pathToId("Models/VRHands/hand_placeholder_l.wmdl");
        auto rHandModel = worlds::AssetDB::pathToId("Models/VRHands/hand_placeholder_r.wmdl");

        entt::entity ent = registry.create();
        registry.emplace<worlds::WorldObject>(ent, matId, leftHand ? lHandModel : rHandModel);
        registry.emplace<Transform>(ent);
        return ent;
    }

    class GrababbleEditor : public worlds::BasicComponentUtil<Grabbable> {
    public:
        BASIC_CREATE(Grabbable);
        BASIC_CLONE(Grabbable);

        void writeToFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file) override {}
        void readFromFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file, int version) override {}

        const char* getName() override { return "Grabbable"; }

        void edit(entt::entity ent, entt::registry& reg, worlds::Editor* ed) override {
            auto& grabbable = reg.get<Grabbable>(ent);
            auto& grabbableTransform = reg.get<Transform>(ent);

            if (ImGui::CollapsingHeader("Grabbable")) {
                if (ImGui::Button("Remove##Grabbable")) {
                    reg.remove<Grabbable>(ent);
                    return;
                }

                if (ImGui::TreeNode("Grips")) {
                    if (ImGui::Button("+")) {
                        grabbable.grips.emplace_back();
                    }

                    static int editingGripIndex = 0;
                    static bool editingGrip = false;
                    static Transform editedGripTransform;
                    static entt::entity previewEntity = entt::null;

                    int eraseIndex = 0;
                    bool erase = false;
                    for (auto [gripIndex, grip] : enumerate(grabbable.grips)) {
                        ImGui::PushID(gripIndex);

                        if (ImGui::Button("-")) {
                            erase = true;
                            eraseIndex = gripIndex;
                        }

                        if (ImGui::BeginCombo("Grip Type", gripTypeNames[(int)grip.gripType])) {
                            int i = 0;
                            for (auto& p : gripTypeNames) {
                                bool isSelected = (int)grip.gripType == i;
                                if (ImGui::Selectable(p, &isSelected)) {
                                    grip.gripType = (GripType)i;
                                }

                                if (isSelected)
                                    ImGui::SetItemDefaultFocus();
                                i++;
                            }
                            ImGui::EndCombo();
                        }

                        if (ImGui::BeginCombo("Hand", gripHandNames[(int)grip.hand])) {
                            int i = 0;
                            for (auto& p : gripHandNames) {
                                bool isSelected = (int)grip.hand == i;
                                if (ImGui::Selectable(p, &isSelected)) {
                                    grip.hand = (GripHand)i;
                                }

                                if (isSelected)
                                    ImGui::SetItemDefaultFocus();
                                i++;
                            }
                            ImGui::EndCombo();
                        }

                        if (!editingGrip) {
                            if (ImGui::Button("Transform")) {
                                editingGrip = true;
                                editingGripIndex = gripIndex;
                                editedGripTransform = Transform { grip.position, grip.rotation }.transformBy(grabbableTransform);
                                if (grip.hand != GripHand::Both)
                                    previewEntity = createHandPreviewEnt(grip.hand == GripHand::Left, reg);
                            }
                        } else if (editingGripIndex == gripIndex) {
                            if (ImGui::Button("Done")) {
                                editingGrip = false;
                                Transform transformedGripTransform = editedGripTransform.transformByInverse(grabbableTransform);
                                grip.position = transformedGripTransform.position;
                                grip.rotation = transformedGripTransform.rotation;
                                if (reg.valid(previewEntity))
                                    reg.destroy(previewEntity);
                            }
                        }

                        ImGui::DragFloat3("Position", glm::value_ptr(grip.position));
                        ImGui::DragFloat4("Rotation", glm::value_ptr(grip.rotation));
                        ImGui::PopID();
                        ImGui::Separator();
                    }

                    if (erase) {
                        grabbable.grips.erase(grabbable.grips.begin() + eraseIndex);
                    }
                    ImGui::TreePop();

                    if (editingGrip) {
                        ed->overrideHandle(&editedGripTransform);
                        if (reg.valid(previewEntity))
                            reg.get<Transform>(previewEntity) = editedGripTransform;
                    }
                }
            }
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override {
            const auto& grabbable = reg.get<Grabbable>(ent);
            json grips = json::array();

            for (const Grip& grip : grabbable.grips) {
                json jsonGrip = {
                    { "gripType", grip.gripType },
                    { "hand", grip.hand },
                    { "position", grip.position },
                    { "rotation", grip.rotation }
                };
                grips.push_back(jsonGrip);
            }

            j = {
                { "grips", grips }
            };
        }

        void fromJson(entt::entity ent, entt::registry& reg, const json& j) override {
            auto& grabbable = reg.emplace<Grabbable>(ent);

            if (j.find("grips") != j.end()) {
                const json& gripArray = j["grips"];

                for (const auto& v : gripArray) {
                    Grip grip;
                    assert(v.is_object());
                    grip.gripType = v["gripType"];
                    grip.hand = v["hand"];
                    grip.position = v["position"];
                    grip.rotation = v["rotation"];
                    grabbable.grips.push_back(grip);
                }
            }
        }
    };

    class GunEditor : public worlds::BasicComponentUtil<Gun> {
    public:
        BASIC_CREATE(Gun);
        BASIC_CLONE(Gun);

        void writeToFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file) override {}
        void readFromFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file, int version) override {}

        const char* getName() override { return "Gun"; }

        void edit(entt::entity ent, entt::registry& reg, worlds::Editor* ed) override {
            static bool editFirePoint = false;
            static Transform firePointTransform;
            auto& gun = reg.get<Gun>(ent);
            auto& gunTransform = reg.get<Transform>(ent);

            if (ImGui::CollapsingHeader("Gun")) {
                if (ImGui::Button("Remove##Gun")) {
                    reg.remove<Gun>(ent);
                    return;
                }

                if (editFirePoint) {
                    if (ImGui::Button("Done")) {
                        editFirePoint = false;
                        gun.firePoint = firePointTransform.transformByInverse(gunTransform);
                    }
                    ed->overrideHandle(&firePointTransform);
                } else {
                    if (ImGui::Button("Edit Fire Point")) {
                        editFirePoint = true;
                        firePointTransform = gun.firePoint.transformBy(gunTransform);
                    }
                }

                ImGui::DragFloat("Shot Period", &gun.shotPeriod);
                ImGui::Checkbox("Automatic", &gun.automatic);
            }
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override {
            auto& gun = reg.get<Gun>(ent);

            j = {
                { "firePoint",
                    {
                        { "position", gun.firePoint.position },
                        { "rotation", gun.firePoint.rotation }
                    }
                },
                { "automatic", gun.automatic },
                { "shotPeriod", gun.shotPeriod }
            };

        }

        void fromJson(entt::entity ent, entt::registry& reg, const json& j) override {
            auto& gun = reg.emplace<Gun>(ent);

            if (j.find("firePoint") != j.end()) {
                gun.firePoint.position = j["firePoint"].value("position", glm::vec3(0.0f));
                gun.firePoint.rotation = j["firePoint"].value("rotation", glm::quat());;
            }

            gun.shotPeriod = j.value("shotPeriod", 0.1f);
            gun.automatic = j.value("automatic", false);
        }
    };

    class DamagingProjectileEditor : public worlds::BasicComponentUtil<DamagingProjectile> {
    public:
        BASIC_CREATE(DamagingProjectile);
        BASIC_CLONE(DamagingProjectile);

        void writeToFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file) override {}
        void readFromFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file, int version) override {}

        const char* getName() override { return "Damaging Projectile"; }

        void edit(entt::entity ent, entt::registry& reg, worlds::Editor* ed) override {
            auto& projectile = reg.get<DamagingProjectile>(ent);

            if (ImGui::CollapsingHeader("DamagingProjectile")) {
                if (ImGui::Button("Remove##DamagingProjectile")) {
                    reg.remove<DamagingProjectile>(ent);
                    return;
                }
                ImGui::DragScalar("Damage", ImGuiDataType_U64, &projectile.damage, 1.0f);
            }
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override {
            auto& projectile = reg.get<DamagingProjectile>(ent);

            j = {
                { "damage", projectile.damage }
            };

        }

        void fromJson(entt::entity ent, entt::registry& reg, const json& j) override {
            auto& projectile = reg.emplace<DamagingProjectile>(ent);

            projectile.damage = j["damage"];
        }
    };

    class StabbyEditor : public worlds::BasicComponentUtil<Stabby> {
    public:
        BASIC_CREATE(Stabby);
        BASIC_CLONE(Stabby);

        void writeToFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file) override {}
        void readFromFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file, int version) override {}

        const char* getName() override { return "Stabby"; }

        void edit(entt::entity ent, entt::registry& reg, worlds::Editor* ed) override {
            auto& stabby = reg.get<Stabby>(ent);

            if (ImGui::CollapsingHeader("Stabby")) {
                if (ImGui::Button("Remove##Stabby")) {
                    reg.remove<Stabby>(ent);
                    return;
                }

                ImGui::DragFloat("Penetration Velocity", &stabby.penetrationVelocity);
                ImGui::DragFloat("Drag Multiplier", &stabby.dragMultiplier);
                ImGui::DragFloat("Drag Floor", &stabby.dragFloor);
                ImGui::DragFloat("Pullout Distance", &stabby.pulloutDistance);
                ImGui::DragFloat3("Stab Direction", glm::value_ptr(stabby.stabDirection));
            }
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override {
            auto& stabby = reg.get<Stabby>(ent);

            j = {
                { "penetrationVelocity", stabby.penetrationVelocity },
                { "dragMultiplier", stabby.dragMultiplier },
                { "dragFloor", stabby.dragFloor },
                { "pulloutDistance", stabby.pulloutDistance },
                { "stabDirection", stabby.stabDirection }
            };

        }

        void fromJson(entt::entity ent, entt::registry& reg, const json& j) override {
            auto& stabby = reg.emplace<Stabby>(ent);

            stabby.penetrationVelocity = j["penetrationVelocity"];
            stabby.dragMultiplier = j["dragMultiplier"];
            stabby.dragFloor = j["dragFloor"];
            stabby.pulloutDistance = j["pulloutDistance"];
            stabby.stabDirection = j["stabDirection"];
        }
    };

    class StabbableEditor : public worlds::BasicComponentUtil<Stabbable> {
    public:
        BASIC_CREATE(Stabbable);
        BASIC_CLONE(Stabbable);

        void writeToFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file) override {}
        void readFromFile(entt::entity ent, entt::registry& reg, PHYSFS_File* file, int version) override {}

        const char* getName() override { return "Stabbable"; }

        void edit(entt::entity ent, entt::registry& reg, worlds::Editor* ed) override {
            auto& stabbable = reg.get<Stabbable>(ent);

            if (ImGui::CollapsingHeader("Stabbable")) {
                if (ImGui::Button("Remove##Stabbable")) {
                    reg.remove<Stabbable>(ent);
                    return;
                }

                if (stabbable.stabSound != ~0u) {
                    ImGui::Text("Stab sound: %s", worlds::AssetDB::idToPath(stabbable.stabSound).c_str());
                } else {
                    ImGui::Text("No stab sound set");
                }
                ImGui::SameLine();
                worlds::selectAssetPopup("Change Stab Sound", stabbable.stabSound, ImGui::Button("Change"));
            }
        }

        void toJson(entt::entity ent, entt::registry& reg, json& j) override {
            auto& stabbable = reg.get<Stabbable>(ent);

            j = json::object();

            if (stabbable.stabSound != ~0u) {
                j["stabSound"] = worlds::AssetDB::idToPath(stabbable.stabSound);
            }
        }

        void fromJson(entt::entity ent, entt::registry& reg, const json& j) override {
            auto& stabbable = reg.emplace<Stabbable>(ent);

            if (j.contains("stabSound")) {
                std::string stabSoundPath = j["stabSound"];
                stabbable.stabSound = worlds::AssetDB::pathToId(stabSoundPath);
            }
        }
    };

    PlayerStartPointEditor pspe;
    PlayerRigEditor pre;
    GripPointEditor gpe;
    PhysicsSoundComponentEditor pse;
    RPGStatsComponentEditor rstatse;
    ContactDamageDealerComponentEditor cddce;
    GrababbleEditor ge;
    GunEditor guned;
    DamagingProjectileEditor dpe;
    StabbyEditor ste;
    StabbableEditor stabbableEditor;

    // Near-empty function to ensure that statics get initialized
    void registerComponentMeta() {
        logMsg("registered game components");
    }
}
