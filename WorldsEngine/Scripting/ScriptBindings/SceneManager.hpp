#include "../WrenVM.hpp"
#include "../ScriptUtil.hpp"
#include "../../Core/Engine.hpp"
#include <entt/core/hashed_string.hpp>

namespace worlds {
    class SceneManagerClass : public ScriptBindClass {
    private:
        static void loadScene(WrenVM* vm) {
            WrenVMData* dat = (WrenVMData*)wrenGetUserData(vm);

            if (wrenGetSlotType(vm, 1) != WREN_TYPE_STRING) {
                throwScriptErr(vm, ("Exepected first argument to be string " + std::to_string(wrenGetSlotType(vm, 1))).c_str());
                return;
            }

            const char* path = wrenGetSlotString(vm, 1);

            if (PHYSFS_exists(path)) {
                dat->interfaces.engine->loadScene(g_assetDB.addOrGetExisting(path));
            } else {
                auto err = "Tried to load non-existent scene " + std::string(path);
                throwScriptErr(vm, err.c_str());
            }
        }

        static void getCurrentScenePath(WrenVM* vm) {
            WrenVMData* dat = (WrenVMData*)wrenGetUserData(vm);
            std::string path = g_assetDB.getAssetPath(dat->interfaces.engine->getCurrentSceneInfo().id);

            wrenSetSlotString(vm, 0, path.c_str());
        }
    public:
        std::string getName() override {
            return "SceneManager";
        }

        WrenForeignMethodFn getFn(bool isStatic, const char* sig) override {
            if (isStatic) {
                if (strcmp(sig, "loadScene(_)") == 0) {
                    return loadScene;
                } else if (strcmp(sig, "getCurrentScenePath()") == 0) {
                    return getCurrentScenePath;
                }
            }

            return nullptr;
        }

        ~SceneManagerClass() override {}
    };

    SceneManagerClass scnManBind;
}
