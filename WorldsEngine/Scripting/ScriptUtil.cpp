#include "ScriptUtil.hpp"
#include <wren.hpp>
#include <glm/vec3.hpp>

namespace worlds {
    void makeVec3(WrenVM* vm, float x, float y, float z, int slot) {
        wrenGetVariable(vm, "worlds_engine/math_types", "Vec3", slot);

        glm::vec3* vPtr = (glm::vec3*)wrenSetSlotNewForeign(vm, slot, slot, sizeof(glm::vec3));
        *vPtr = glm::vec3{ x, y, z };
    }

    void throwScriptErr(WrenVM* vm, const char* msg) {
        wrenSetSlotString(vm, 0, msg); 
        wrenAbortFiber(vm, 0);
    }
}
