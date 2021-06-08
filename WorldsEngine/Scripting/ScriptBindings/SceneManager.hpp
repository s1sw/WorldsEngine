#include "../WrenVM.hpp"
#include "../ScriptUtil.hpp"
#include "../../Core/Engine.hpp"
#include "../../Serialization/SceneSerialization.hpp"
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
                dat->interfaces.engine->loadScene(AssetDB::pathToId(path));
            } else {
                auto err = "Tried to load non-existent scene " + std::string(path);
                throwScriptErr(vm, err.c_str());
            }
        }

        static void createPrefab(WrenVM* vm) {
            WrenVMData* dat = (WrenVMData*)wrenGetUserData(vm);

            if (wrenGetSlotType(vm, 1) != WREN_TYPE_STRING) {
                throwScriptErr(vm, ("Exepected first argument to be string " + std::to_string(wrenGetSlotType(vm, 1))).c_str());
                return;
            }

            const char* path = wrenGetSlotString(vm, 1);

            if (PHYSFS_exists(path)) {
                SceneLoader::createPrefab(AssetDB::pathToId(path), dat->reg);
            } else {
                auto err = "Tried to create non-existent prefab " + std::string(path);
                throwScriptErr(vm, err.c_str());
            }
        }

        static void getCurrentScenePath(WrenVM* vm) {
            WrenVMData* dat = (WrenVMData*)wrenGetUserData(vm);
            std::string path = AssetDB::idToPath(dat->interfaces.engine->getCurrentSceneInfo().id);

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
                } else if (strcmp(sig, "createPrefab(_)") == 0) {
                    return createPrefab;
                }
            }

            return nullptr;
        }

        ~SceneManagerClass() override {}
    };

    SceneManagerClass scnManBind;
}
