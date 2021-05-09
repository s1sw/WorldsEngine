#include "SceneSerialization.hpp"
#include "../Core/Log.hpp"
#include <physfs.h>

namespace worlds {
    // Do basic checks on the first byte to determine
    // the most appropriate scene serializer to call.
    void SceneLoader::loadScene(PHYSFS_File* file, entt::registry& reg, bool additive) {
        unsigned char firstByte;
        PHYSFS_readBytes(file, &firstByte, 1);
        PHYSFS_seek(file, 0);

        // check first character of magic to determine WSCN or ESCN
        if (firstByte == 'W' || firstByte == 'E') {
            if (!additive)
                reg.clear();

            BinarySceneSerializer::loadScene(file, reg);
        } else if (firstByte == '{') {
            if (!additive)
                reg.clear();

            JsonSceneSerializer::loadScene(file, reg);
        } else {
            logErr(WELogCategoryEngine, "Scene has unrecognized file format");
            PHYSFS_close(file);
            return;
        }

        PHYSFS_close(file);
    }
}
