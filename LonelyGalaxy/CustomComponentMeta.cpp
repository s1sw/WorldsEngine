#include <entt/entt.hpp>
#include <ComponentMeta/ComponentEditorUtil.hpp>
#include <Core/Log.hpp>
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
                    ImGui::Text("Current Asset Path: %s", worlds::g_assetDB.getAssetPath(psc.soundId).c_str());
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
                { "soundPath", worlds::g_assetDB.getAssetPath(psc.soundId) }
            };
        }

        void fromJson(entt::entity ent, entt::registry& reg, const json& j) override {
            auto& psc = reg.emplace<PhysicsSoundComponent>(ent);

            psc.soundId = worlds::g_assetDB.addOrGetExisting(j["soundPath"]);
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

    PlayerStartPointEditor pspe;
    PlayerRigEditor pre;
    GripPointEditor gpe;
    PhysicsSoundComponentEditor pse;
    RPGStatsComponentEditor rstatse;
    ContactDamageDealerComponentEditor cddce;

    // Near-empty function to ensure that statics get initialized
    void registerComponentMeta() {
        logMsg("registered game components");
    }
}
