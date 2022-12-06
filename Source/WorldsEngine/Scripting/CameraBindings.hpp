#include "Core/Log.hpp"
#include "Export.hpp"
#include "Render/Camera.hpp"

using namespace worlds;

Camera* csharpMainCamera;
extern "C"
{
    EXPORT void camera_getPosition(Camera* cam, glm::vec3* pos)
    {
        *pos = cam->position;
    }

    EXPORT void camera_setPosition(Camera* cam, glm::vec3* pos)
    {
        cam->position = *pos;
    }

    EXPORT void camera_getRotation(Camera* cam, glm::quat* rotation)
    {
        *rotation = cam->rotation;
    }

    EXPORT void camera_setRotation(Camera* cam, glm::quat* rotation)
    {
        cam->rotation = *rotation;
    }

    EXPORT Camera* camera_getMain()
    {
        return csharpMainCamera;
    }
}
