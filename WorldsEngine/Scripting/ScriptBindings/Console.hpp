#include "../WrenVM.hpp"
#include "../ScriptUtil.hpp"
#include "../../Core/Console.hpp"

namespace worlds {
    class ConsoleBinding : public ScriptBindClass {
    private:
        static void executeCommand(WrenVM* vm) {
            WrenVMData* dat = (WrenVMData*)wrenGetUserData(vm);

            if (wrenGetSlotType(vm, 1) != WREN_TYPE_STRING) {
                throwScriptErr(vm, ("Exepected first argument to be string " + std::to_string(wrenGetSlotType(vm, 1))).c_str());
                return;
            }

            const char* cmd = wrenGetSlotString(vm, 1);
            g_console->executeCommandStr(cmd, false);
        }
    public:
        std::string getName() override {
            return "Console";
        }

        WrenForeignMethodFn getFn(bool isStatic, const char* sig) override {
            if (isStatic) {
                if (strcmp(sig, "executeCommand(_)") == 0) {
                    return executeCommand;
                }
            }

            return nullptr;
        }

        ~ConsoleBinding() override {}
    };

    ConsoleBinding cb{};
}
