#include "../WrenVM.hpp"
#include "../../Core/Engine.hpp"

namespace worlds {
    class LightBinding : public ScriptBindClass {
    private:
        static void setEnabled(WrenVM* vm) {
            wrenEnsureSlots(vm, 1);
            WrenVMData* dat = (WrenVMData*)wrenGetUserData(vm);

            uint32_t entId = *(uint32_t*)wrenGetSlotForeign(vm, 0);

            WorldLight& wl = dat->reg.get<WorldLight>((entt::entity)entId);
            wl.enabled = wrenGetSlotBool(vm, 1);
        }

        static void getEnabled(WrenVM* vm) {
            wrenEnsureSlots(vm, 1);
            WrenVMData* dat = (WrenVMData*)wrenGetUserData(vm);

            uint32_t entId = *(uint32_t*)wrenGetSlotForeign(vm, 0);

            WorldLight& wl = dat->reg.get<WorldLight>((entt::entity)entId);
            wrenSetSlotBool(vm, 0, wl.enabled);
        }
    public:
        std::string getName() override {
            return "Light";
        }

        WrenForeignMethodFn getFn(bool isStatic, const char* sig) override {
            if (!isStatic) {
                if (strcmp(sig, "getEnabled()") == 0) {
                    return getEnabled;
                } else if (strcmp(sig, "setEnabled(_)") == 0) {
                    return setEnabled;
                }
            }

            return nullptr;
        }

        ~LightBinding() {}
    };

    LightBinding lb{};
}
