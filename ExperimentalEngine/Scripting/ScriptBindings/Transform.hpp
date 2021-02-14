#pragma once
#include "WrenVM.hpp"
#include "../Transform.hpp"
#include <entt/entity/registry.hpp>
#include "ScriptUtil.hpp"

namespace worlds {
    class TransformBinding : public ScriptBindClass {
    private:
        static void getPosition(WrenVM* vm) {
            wrenEnsureSlots(vm, 1);
            WrenVMData* dat = (WrenVMData*)wrenGetUserData(vm);

            uint32_t entId = *(uint32_t*)wrenGetSlotForeign(vm, 0);

            Transform& t = dat->reg.get<Transform>((entt::entity)entId);

            makeVec3(vm, t.position.x, t.position.y, t.position.z);
        }

        static void setPosition(WrenVM* vm) {
            wrenEnsureSlots(vm, 1);
            WrenVMData* dat = (WrenVMData*)wrenGetUserData(vm);

            uint32_t entId = *(uint32_t*)wrenGetSlotForeign(vm, 0);
            glm::vec3* posPtr = (glm::vec3*)wrenGetSlotForeign(vm, 1);

            Transform& t = dat->reg.get<Transform>((entt::entity)entId);

            t.position = *posPtr;
        }
    public:
        std::string getName() override {
            return "Transform";
        }

        WrenForeignMethodFn getFn(bool isStatic, const char* sig) override {
            if (!isStatic) {
                if (strcmp(sig, "getPosition()") == 0) {
                    return getPosition;
                } else if (strcmp(sig, "setPosition(_)") == 0) {
                    return setPosition;
                }
            }

            return nullptr;
        }
    };
    TransformBinding tfBinding;
}
