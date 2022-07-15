#pragma once
#include "entt/entity/fwd.hpp"
#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace lg
{
    class DebugArrows
    {
      public:
        DebugArrows(entt::registry &reg);
        void drawArrow(glm::vec3 start, glm::vec3 dir);
        void newFrame();
        void createEntities();
        void destroyEntities();

      private:
        entt::registry &reg;
        std::vector<entt::entity> arrowEntities;
        size_t arrowsInUse;
    };

    extern DebugArrows *g_dbgArrows;
}
