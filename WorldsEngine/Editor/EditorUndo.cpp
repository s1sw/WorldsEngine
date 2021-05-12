#include "Editor.hpp"
#include "../Serialization/SceneSerialization.hpp"

namespace worlds {
    void EditorUndo::pushState(entt::registry& reg) {
        // Serialize to file
        std::string savedPath = "Temp/" + std::to_string(currentPos) + ".json";
        PHYSFS_File* file = PHYSFS_openWrite(savedPath.c_str());
        JsonSceneSerializer::saveScene(file, reg);
        highestSaved = currentPos;
        currentPos++;
    }

    void EditorUndo::undo(entt::registry& reg) {
        if (currentPos == 0)
            return;
        currentPos--;
        std::string path = "Temp/" + std::to_string(currentPos) + ".json";
        PHYSFS_File* file = PHYSFS_openRead(path.c_str());
        reg.clear();
        JsonSceneSerializer::loadScene(file, reg);
        PHYSFS_close(file);
    }

    void EditorUndo::redo(entt::registry& reg) {
        std::string path = "Temp/" + std::to_string(currentPos + 1) + ".json";
        if (!PHYSFS_exists(path.c_str()) || currentPos == highestSaved)
            return;
        currentPos++;
        PHYSFS_File* file = PHYSFS_openRead(path.c_str());
        reg.clear();
        JsonSceneSerializer::loadScene(file, reg);
    }
}
