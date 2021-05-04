#pragma once
#include <openvr.h>
#include <vector>
#include <string>
#include <sstream>
#include <vulkan/vulkan.h>
#include <SDL_messagebox.h>
#include <glm/gtx/matrix_decompose.hpp>
#include <filesystem>
#include <SDL_filesystem.h>
#include "../Core/Fatal.hpp"
#include "../Util/MatUtil.hpp"
#include "../Core/Transform.hpp"
#include "IVRInterface.hpp"
#include "../Core/Log.hpp"
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
        vr::VRActionHandle_t leftHandSkeletal;
        vr::VRActionHandle_t rightHandSkeletal;
        vr::VRActionHandle_t sprintAction;
        vr::VRActionHandle_t jumpAction;
        vr::VRActionSetHandle_t actionSet;

        void checkErr(vr::EVRInputError err);
        uint32_t handBoneCount;
        vr::VRBoneTransform_t* handBoneArray;
    public:
        void init();

        std::vector<std::string> getVulkanInstanceExtensions();
        std::vector<std::string> getVulkanDeviceExtensions(VkPhysicalDevice physDevice);

        void getRenderResolution(uint32_t* x, uint32_t* y);

        static glm::mat4 toMat4(vr::HmdMatrix34_t mat);

        static glm::mat4 toMat4(vr::HmdMatrix44_t mat);

        glm::mat4 getViewMat(vr::EVREye eye);

        glm::mat4 getProjMat(vr::EVREye eye, float near);
        glm::mat4 getProjMat(vr::EVREye eye, float near, float far);
        void updateInput() override;
        bool getHandTransform(Hand hand, Transform& t) override;
        glm::mat4 getHeadTransform() override;
        glm::vec2 getLocomotionInput() override;

        bool getJumpInput() override;
        bool getSprintInput() override;
        InputActionHandle getActionHandle(std::string actionPath) override;
        bool getActionHeld(InputActionHandle handle) override;
        bool getActionPressed(InputActionHandle handle) override;
        bool getActionReleased(InputActionHandle handle) override;
        glm::vec2 getActionV2(InputActionHandle handle) override;
    };
}
