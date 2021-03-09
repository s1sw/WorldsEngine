#include "../WrenVM.hpp"
#include <entt/core/hashed_string.hpp>

namespace worlds {
    class EntityBinding : public ScriptBindClass {
    private:
        static void getTransform(WrenVM* vm) {
            uint32_t entId = *(uint32_t*)wrenGetSlotForeign(vm, 0);

            wrenEnsureSlots(vm, 1);
            wrenGetVariable(vm, "worlds_engine/entity", "Transform", 0);

            uint32_t* idPtr = (uint32_t*)wrenSetSlotNewForeign(vm, 0, 0, sizeof(uint32_t));
            *idPtr = entId;
        }

        static void getDynamicPhysicsActor(WrenVM* vm) {
            uint32_t entId = *(uint32_t*)wrenGetSlotForeign(vm, 0);

            wrenEnsureSlots(vm, 1);
            wrenGetVariable(vm, "worlds_engine/entity", "DynamicPhysicsActor", 0);

            uint32_t* idPtr = (uint32_t*)wrenSetSlotNewForeign(vm, 0, 0, sizeof(uint32_t));
            *idPtr = entId;
        }

        static void getId(WrenVM* vm) {
            uint32_t entId = *(uint32_t*)wrenGetSlotForeign(vm, 0);

            wrenEnsureSlots(vm, 1);
            wrenSetSlotDouble(vm, 0, entId);
        }
    public:
        std::string getName() override {
            return "Entity";
        }

        WrenForeignMethodFn getFn(bool isStatic, const char* sig) override {
            if (!isStatic) {
                if (strcmp(sig, "getTransform()") == 0) {
                    return getTransform;
                } else if (strcmp(sig, "getDynamicPhysicsActor()") == 0) {
                    return getDynamicPhysicsActor;
                } else if (strcmp(sig, "getId()") == 0) {
                    return getId;
                }
            }

            return nullptr;
        }
    };

    EntityBinding eb{};
}
