#include "DebugArrow.hpp"
#include "MathsUtil.hpp"
#include "Core/AssetDB.hpp"
#include <Util/CreateModelObject.hpp>

namespace lg {
    DebugArrows* g_dbgArrows;
    DebugArrows::DebugArrows(entt::registry& reg) : reg(reg), arrowsInUse(0) {
        g_dbgArrows = this;
    }

    void DebugArrows::drawArrow(glm::vec3 start, glm::vec3 dir) {
        glm::vec3 ndir = glm::normalize(dir);
        glm::quat q = safeQuatLookat(ndir);

        auto ent = arrowEntities[arrowsInUse];
        arrowsInUse++;

        auto& t = reg.get<Transform>(ent);
        t.position = start;
        t.rotation = q;
    }

    void DebugArrows::newFrame() {
        for (auto& ent : arrowEntities) {
            auto& t = reg.get<Transform>(ent);
            t.position = glm::vec3(0.0f, -10000.0f, 0.0f);
        }

        arrowsInUse = 0;
    }
    
    void DebugArrows::createEntities() {
        arrowEntities.clear();
        auto meshId = worlds::AssetDB::pathToId("arrow.obj");
        auto matId = worlds::AssetDB::pathToId("Materials/glowred.json");
        for (size_t i = 0; i < 16; i++) {
            auto ent = worlds::createModelObject(reg, glm::vec3{0.0f}, glm::quat{}, meshId, matId);

            auto& t = reg.get<Transform>(ent); 
            t.position = glm::vec3(0.0f, -10000.0f, 0.0f);

            arrowEntities.push_back(ent);
        }
    }

    void DebugArrows::destroyEntities() {
        arrowEntities.clear();
    }
}
