#include "../Serialization/SceneSerialization.hpp"
#include "Editor.hpp"

namespace worlds
{
    void EditorUndo::pushState(entt::registry& reg)
    {
        // Serialize to file
        std::string savedPath = "Temp/" + std::to_string(currentPos) + ".json";
        JsonSceneSerializer::saveScene(savedPath, reg);
        highestSaved = currentPos;
        currentPos++;
    }

    void EditorUndo::undo(entt::registry& reg)
    {
        if (currentPos == 0)
            return;
        currentPos--;
        std::string path = "Temp/" + std::to_string(currentPos) + ".json";
        PHYSFS_File* file = PHYSFS_openRead(path.c_str());
        reg.clear();
        JsonSceneSerializer::loadScene(file, reg);
        PHYSFS_close(file);
    }

    void EditorUndo::redo(entt::registry& reg)
    {
        std::string path = "Temp/" + std::to_string(currentPos + 1) + ".json";
        if (!PHYSFS_exists(path.c_str()) || currentPos == highestSaved)
            return;
        currentPos++;
        PHYSFS_File* file = PHYSFS_openRead(path.c_str());
        reg.clear();
        JsonSceneSerializer::loadScene(file, reg);
    }

    void EditorUndo::clear()
    {
        highestSaved = 0;
        currentPos = 0;

        PHYSFS_enumerate(
            "Temp",
            [](void* data, const char*, const char* name) {
                char* buf = new char[strlen("Temp/") + strlen(name) + 1];
                buf[0] = 0;
                strcat(buf, "Temp/");
                strcat(buf, name);
                PHYSFS_delete(buf);
                delete[] buf;

                return PHYSFS_ENUM_OK;
            },
            nullptr);
    }
}
