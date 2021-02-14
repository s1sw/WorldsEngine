#include "OpenVRInterface.hpp"

namespace worlds {
    void OpenVRInterface::checkErr(vr::EVRInputError err) {
        if (err != vr::VRInputError_None) {
            logErr("VR Input Error: %i", err);
        }
    }

    void OpenVRInterface::init() {
        vr::EVRInitError eError = vr::VRInitError_None;
        system = vr::VR_Init(&eError, vr::VRApplication_Scene);

        if (eError != vr::VRInitError_None) {
            std::string errMsg = std::string{"Unable to init VR runtime: "} + vr::VR_GetVRInitErrorAsEnglishDescription(eError);
            fatalErr(errMsg.c_str());
        }
        
        const char* basePath = SDL_GetBasePath();
        

        std::string actionsPathStr = basePath + std::string("EEData") + PATH_SEP + "actions.json";
        const char* actionsPath = actionsPathStr.c_str();
        logMsg("Using %s as actionsPath", actionsPath);
        auto vrInput = vr::VRInput();
        vrInput->SetActionManifestPath(actionsPath);
        checkErr(vrInput->GetActionHandle("/actions/main/in/Movement", &movementAction));
        checkErr(vrInput->GetActionHandle("/actions/main/in/LeftHand", &leftHand));
        checkErr(vrInput->GetActionHandle("/actions/main/in/RightHand", &rightHand));
        checkErr(vrInput->GetActionHandle("/actions/main/in/Sprint", &sprintAction));
        checkErr(vrInput->GetActionHandle("/actions/main/in/Jump", &jumpAction));

        checkErr(vrInput->GetActionSetHandle("/actions/main", &actionSet));
    }

    std::vector<std::string> OpenVRInterface::getVulkanInstanceExtensions() {
        uint32_t extCharCount = vr::VRCompositor()->GetVulkanInstanceExtensionsRequired(nullptr, 0);

        std::string str((size_t)extCharCount, 'Z');

        vr::VRCompositor()->GetVulkanInstanceExtensionsRequired(str.data(), (uint32_t)str.size());

        std::vector<std::string> extensions;

        std::istringstream stringStream(str.substr(0, extCharCount));
        std::string currExt;
        while (std::getline(stringStream, currExt, ' ')) {
            extensions.push_back(currExt);
        }

        return extensions;
    }

    std::vector<std::string> OpenVRInterface::getVulkanDeviceExtensions(VkPhysicalDevice physDevice) {
        uint32_t extCharCount = vr::VRCompositor()->GetVulkanDeviceExtensionsRequired(physDevice, nullptr, 0);

        std::string str((size_t)extCharCount, 'Z');

        vr::VRCompositor()->GetVulkanDeviceExtensionsRequired(physDevice, str.data(), (uint32_t)str.size());

        std::vector<std::string> extensions;

        std::istringstream stringStream(str);
        std::string currExt;
        while (std::getline(stringStream, currExt, ' ')) {
            extensions.push_back(currExt);
        }

        return extensions;
    }

    void composeProjection(float fLeft, float fRight, float fTop, float fBottom, float zNear, glm::mat4& p) {
        float idx = 1.0f / (fRight - fLeft);
        float idy = 1.0f / (fBottom - fTop);
        float sx = fRight + fLeft;
        float sy = fBottom + fTop;

        p[0][0] = 2 * idx; p[1][0] = 0;       p[2][0] = sx * idx;    p[3][0] = 0;
        p[0][1] = 0;       p[1][1] = 2 * idy; p[2][1] = sy * idy;    p[3][1] = 0;
        p[0][2] = 0;       p[1][2] = 0;       p[2][2] = 0.0f; p[3][2] = zNear;
        p[0][3] = 0;       p[1][3] = 0;       p[2][3] = -1.0f;       p[3][3] = 0;
    }

    void OpenVRInterface::getRenderResolution(uint32_t* x, uint32_t* y) {
        system->GetRecommendedRenderTargetSize(x, y);
    }

    glm::mat4 OpenVRInterface::getProjMat(vr::EVREye eye, float near, float) {
        float left, right, top, bottom;
        system->GetProjectionRaw(eye, &left, &right, &top, &bottom);

        glm::mat4 m;

        composeProjection(left, right, top, bottom, near, m);
        return m;
    }

    void OpenVRInterface::updateInput() {
        vr::VRActiveActionSet_t activeActionSet{
            .ulActionSet = actionSet
        };
        vr::VRInput()->UpdateActionState(&activeActionSet, sizeof(activeActionSet), 1);
    }

    bool OpenVRInterface::getHandTransform(Hand hand, Transform& t) {
        vr::InputPoseActionData_t pose;

        auto retVal = vr::VRInput()->GetPoseActionDataForNextFrame(hand == Hand::LeftHand ? leftHand : rightHand, vr::TrackingUniverseStanding, &pose, sizeof(pose), vr::k_ulInvalidInputValueHandle);

        if (retVal != vr::VRInputError_None)
            return false;

        glm::mat4 matrix = toMat4(pose.pose.mDeviceToAbsoluteTracking);

        t.position = getMatrixTranslation(matrix);
        t.rotation = getMatrixRotation(matrix);

        if (glm::any(glm::isnan(t.position)) || glm::any(glm::isnan(t.rotation)))
            fatalErr("controller stuff was NaN?!?!?!?");

        return true;
    }

    glm::mat4 OpenVRInterface::getHeadTransform() {
        vr::TrackedDevicePose_t hmdPose;
        system->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, 0.0f, &hmdPose, 1);
        return toMat4(hmdPose.mDeviceToAbsoluteTracking);
    }

    glm::vec2 OpenVRInterface::getLocomotionInput() {
        vr::InputAnalogActionData_t data;
        vr::VRInput()->GetAnalogActionData(movementAction, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);
        return glm::vec2(data.x, data.y);
    }

    bool OpenVRInterface::getJumpInput() {
        vr::InputDigitalActionData_t data;
        vr::VRInput()->GetDigitalActionData(jumpAction, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);

        return data.bChanged && data.bState;
    }

    bool OpenVRInterface::getSprintInput() {
        vr::InputDigitalActionData_t data;
        vr::VRInput()->GetDigitalActionData(sprintAction, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);

        return data.bState;
    }

    InputActionHandle OpenVRInterface::getActionHandle(std::string actionPath) {
        vr::VRActionHandle_t handle;
        vr::VRInput()->GetActionHandle(actionPath.c_str(), &handle);

        return handle;
    }

    bool OpenVRInterface::getActionHeld(InputActionHandle handle) {
        vr::InputDigitalActionData_t data;
        vr::VRInput()->GetDigitalActionData(handle, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);

        return data.bState;
    }

    bool OpenVRInterface::getActionPressed(InputActionHandle handle) {
        vr::InputDigitalActionData_t data;
        vr::VRInput()->GetDigitalActionData(handle, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);
        return data.bState && data.bChanged;
    }

    bool OpenVRInterface::getActionReleased(InputActionHandle handle) {
        vr::InputDigitalActionData_t data;
        vr::VRInput()->GetDigitalActionData(handle, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);

        return !data.bState && data.bChanged;
    }
}
