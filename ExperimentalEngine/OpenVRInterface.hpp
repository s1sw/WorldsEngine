#pragma once
#include <openvr.h>
#include <vector>
#include <string>
#include <sstream>
#include <vulkan/vulkan.h>
#include <SDL2/SDL_messagebox.h>
#include <glm/gtx/matrix_decompose.hpp>
#include <filesystem>
#include <SDL2/SDL_filesystem.h>
#include "MatUtil.hpp"
#include "Transform.hpp"
#include "IVRInterface.hpp"
#include "Log.hpp"
#ifdef _WIN32
const char PATH_SEP = '\\';
#else
const char PATH_SEP = '/';
#endif

namespace worlds {
    class OpenVRInterface : public IVRInterface {
        vr::IVRSystem* system;
        vr::VRActionHandle_t movementAction;
        vr::VRActionHandle_t leftHand;
        vr::VRActionHandle_t rightHand;
        vr::VRActionHandle_t sprintAction;
        vr::VRActionHandle_t jumpAction;
        vr::VRActionSetHandle_t actionSet;

        void checkErr(vr::EVRInputError err) {
            if (err != vr::VRInputError_None) {
                logErr("VR Input Error: %i", err);
            }
        }
    public:
        void init() {
            vr::EVRInitError eError = vr::VRInitError_None;
            system = vr::VR_Init(&eError, vr::VRApplication_Scene);

            if (eError != vr::VRInitError_None) {
                //sprintf_s(buf, sizeof(buf), "Unable to init VR runtime: %s", vr::VR_GetVRInitErrorAsEnglishDescription(eError));
                std::string errMsg = std::string{"Unable to init VR runtime: "} + vr::VR_GetVRInitErrorAsEnglishDescription(eError);
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "VR_Init Failed", errMsg.c_str(), NULL);
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

        std::vector<std::string> getVulkanInstanceExtensions() {
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

        std::vector<std::string> getVulkanDeviceExtensions(VkPhysicalDevice physDevice) {
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

        void getRenderResolution(uint32_t* x, uint32_t* y) {
            system->GetRecommendedRenderTargetSize(x, y);
        }

        glm::mat4 toMat4(vr::HmdMatrix34_t mat) {
            return glm::mat4(
                mat.m[0][0], mat.m[1][0], mat.m[2][0], 0.0f,
                mat.m[0][1], mat.m[1][1], mat.m[2][1], 0.0f,
                mat.m[0][2], mat.m[1][2], mat.m[2][2], 0.0f,
                mat.m[0][3], mat.m[1][3], mat.m[2][3], 1.0f);
        }

        glm::mat4 toMat4(vr::HmdMatrix44_t mat) {
            return glm::mat4(
                mat.m[0][0], mat.m[1][0], mat.m[2][0], mat.m[3][0],
                mat.m[0][1], mat.m[1][1], mat.m[2][1], mat.m[3][1],
                mat.m[0][2], mat.m[1][2], mat.m[2][2], mat.m[3][2],
                mat.m[0][3], mat.m[1][3], mat.m[2][3], mat.m[3][3]);
        }

        glm::mat4 getViewMat(vr::EVREye eye) {
            return toMat4(system->GetEyeToHeadTransform(eye));
        }

        void ComposeProjection(float fLeft, float fRight, float fTop, float fBottom, float zNear, float zFar, glm::mat4& p) {
            float idx = 1.0f / (fRight - fLeft);
            float idy = 1.0f / (fBottom - fTop);
            float sx = fRight + fLeft;
            float sy = fBottom + fTop;

            p[0][0] = 2 * idx; p[1][0] = 0;       p[2][0] = sx * idx;    p[3][0] = 0;
            p[0][1] = 0;       p[1][1] = 2 * idy; p[2][1] = sy * idy;    p[3][1] = 0;
            p[0][2] = 0;       p[1][2] = 0;       p[2][2] = 0.0f; p[3][2] = zNear;
            p[0][3] = 0;       p[1][3] = 0;       p[2][3] = -1.0f;       p[3][3] = 0;
        }

        glm::mat4 getProjMat(vr::EVREye eye, float near, float far) {
            float left, right, top, bottom;
            system->GetProjectionRaw(eye, &left, &right, &top, &bottom);

            glm::mat4 m;

            ComposeProjection(left, right, top, bottom, near, far, m);
            return m;
        }

        void updateInput() override {
            vr::VRActiveActionSet_t activeActionSet{
                .ulActionSet = actionSet
            };
            vr::VRInput()->UpdateActionState(&activeActionSet, sizeof(activeActionSet), 1);
        }

        bool getHandTransform(vr::ETrackedControllerRole role, Transform& t) {
            vr::InputPoseActionData_t pose;

            auto retVal = vr::VRInput()->GetPoseActionDataForNextFrame(role == vr::TrackedControllerRole_LeftHand ? leftHand : rightHand, vr::TrackingUniverseStanding, &pose, sizeof(pose), vr::k_ulInvalidInputValueHandle);

            if (retVal != vr::VRInputError_None)
                return false;

            glm::mat4 matrix = toMat4(pose.pose.mDeviceToAbsoluteTracking);

            t.position = getMatrixTranslation(matrix);
            t.rotation = getMatrixRotation(matrix);

            return true;
        }

        glm::mat4 getHeadTransform() override {
            vr::TrackedDevicePose_t hmdPose;
            system->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, 0.0f, &hmdPose, 1);
            return toMat4(hmdPose.mDeviceToAbsoluteTracking);
        }

        glm::vec2 getLocomotionInput() override {
            vr::InputAnalogActionData_t data;
            vr::VRInput()->GetAnalogActionData(movementAction, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);
            return glm::vec2(data.x, data.y);
        }

        bool getJumpInput() override {
            vr::InputDigitalActionData_t data;
            vr::VRInput()->GetDigitalActionData(jumpAction, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);

            return data.bChanged && data.bActive;
        }

        bool getSprintInput() override {
            vr::InputDigitalActionData_t data;
            vr::VRInput()->GetDigitalActionData(sprintAction, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);

            return data.bState;
        }

        InputActionHandle getActionHandle(std::string actionPath) override {
            vr::VRActionHandle_t handle;
            vr::VRInput()->GetActionHandle(actionPath.c_str(), &handle);

            return handle;
        }

        bool getActionHeld(InputActionHandle handle) override {
            vr::InputDigitalActionData_t data;
            vr::VRInput()->GetDigitalActionData(handle, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);

            return data.bState;
        }

        bool getActionPressed(InputActionHandle handle) override {
            vr::InputDigitalActionData_t data;
            vr::VRInput()->GetDigitalActionData(handle, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);

            return data.bState && data.bChanged;
        }

        bool getActionReleased(InputActionHandle handle) override {
            vr::InputDigitalActionData_t data;
            vr::VRInput()->GetDigitalActionData(handle, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);

            return !data.bState && data.bChanged;
        }
    };
}
