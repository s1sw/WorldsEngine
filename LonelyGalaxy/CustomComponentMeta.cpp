#include <entt/entt.hpp>
#include <ComponentMeta/ComponentEditorUtil.hpp>
#include <Core/Log.hpp>
#include "PlayerStartPoint.hpp"
#include "LocospherePlayerSystem.hpp"
#include <ImGui/imgui.h>

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
    };

    PlayerStartPointEditor pspe;

    // Near-empty function to ensure that statics get initialized
    void registerComponentMeta() {
        logMsg("registered game components");
    }
}
