#include "Export.hpp"
#include "VR/IVRInterface.hpp"

using namespace worlds;
IVRInterface* csharpVrInterface;

extern "C" {
    EXPORT bool vr_enabled() {
        return csharpVrInterface != nullptr;
    }

    EXPORT void vr_getHeadTransform(float predictionTime, Transform* transform) {
        transform->fromMatrix(csharpVrInterface->getHeadTransform(predictionTime));
    }
}
