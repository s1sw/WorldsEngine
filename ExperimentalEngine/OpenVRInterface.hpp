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
                char buf[1024];
                sprintf_s(buf, sizeof(buf), "Unable to init VR runtime: %s", vr::VR_GetVRInitErrorAsEnglishDescription(eError));
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "VR_Init Failed", buf, NULL);
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

        glm::mat4 getProjMat(vr::EVREye eye, float near, float far) {
            return toMat4(system->GetProjectionMatrix(eye, near, far));
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
    };
}