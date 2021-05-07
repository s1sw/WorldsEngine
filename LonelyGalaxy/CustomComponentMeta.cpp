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

using json = nlohmann::json;

namespace lg {
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

    PlayerStartPointEditor pspe;
    PlayerRigEditor pre;
    GripPointEditor gpe;
    PhysicsSoundComponentEditor pse;

    // Near-empty function to ensure that statics get initialized
    void registerComponentMeta() {
        logMsg("registered game components");
    }
}
