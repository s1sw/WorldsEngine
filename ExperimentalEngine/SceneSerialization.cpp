#include "SceneSerialization.hpp"
#include "AssetDB.hpp"
#include "Engine.hpp"
#include "Transform.hpp"
#include "PhysicsActor.hpp"
#include "Physics.hpp"
#include <filesystem>
#include "Log.hpp"
#include "TimingUtil.hpp"
#include "SceneSerializationFuncs.hpp"

namespace worlds {

    typedef void(*LoadSceneFunc)(AssetID, PHYSFS_File*, entt::registry&, bool);

    const LoadSceneFunc idFuncs[] = {nullptr, loadScene01, loadScene02};

    const unsigned char LATEST_SCN_FORMAT_ID = 2;
    const unsigned char SCN_FORMAT_MAGIC[5] = { 'E','S','C','N', '\0' };
    const int MAX_FORMAT_ID = 2;

    void loadScene(AssetID id, entt::registry& reg, bool additive) {
        char magicCheck[5];
        magicCheck[4] = 0;
        PHYSFS_File* file = g_assetDB.openAssetFileRead(id);
        PHYSFS_readBytes(file, magicCheck, 4);
        unsigned char formatId;
        PHYSFS_readBytes(file, &formatId, sizeof(formatId));

        if (formatId > MAX_FORMAT_ID) {
            logErr(WELogCategoryEngine, "scene has incompatible format id: got %i, expected %i or lower", formatId, MAX_FORMAT_ID);
            return;
        }

        if (memcmp(magicCheck, SCN_FORMAT_MAGIC, 4) != 0) {
            logErr(WELogCategoryEngine, "failed magic check: got %s, expected", magicCheck, SCN_FORMAT_MAGIC);
            return;
        }

        logMsg("Loading scene version %i", formatId);

        idFuncs[formatId](id, file, reg, additive);

        PHYSFS_close(file);
    }
}