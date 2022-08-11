#pragma once
#include "../Core/Fatal.hpp"
#include "../Core/Log.hpp"
#include "../Core/Transform.hpp"
#include "../Util/MatUtil.hpp"
#include "IVRInterface.hpp"
#include <SDL_filesystem.h>
#include <SDL_messagebox.h>
#include <filesystem>
#include <glm/gtx/matrix_decompose.hpp>
#include <openvr.h>
#include <sstream>
#include <string>
#include <vector>
#ifdef _WIN32
const char PATH_SEP = '\\';
#else
const char PATH_SEP = '/';
#endif

#define VK_DEFINE_HANDLE(object) typedef struct object##_T* object;
VK_DEFINE_HANDLE(VkPhysicalDevice)
#undef VK_DEFINE_HANDLE

namespace worlds
{
    class OpenVRInterface : public IVRInterface
    {
        vr::IVRSystem* system;
        vr::VRActionHandle_t movementAction;
        vr::VRActionHandle_t leftHand;
        vr::VRActionHandle_t rightHand;
        vr::VRActionHandle_t leftHandSkeletal;
        vr::VRActionHandle_t rightHandSkeletal;
        vr::VRActionHandle_t sprintAction;
        vr::VRActionHandle_t jumpAction;
        vr::VRActionSetHandle_t actionSet;

        uint32_t handBoneCount;
        vr::VRBoneTransform_t* lhandBoneArray;
        vr::VRBoneTransform_t* rhandBoneArray;

        bool hasInputFocus = true;

      public:
        void init();

        std::vector<std::string> getVulkanInstanceExtensions() override;
        std::vector<std::string> getVulkanDeviceExtensions(VkPhysicalDevice physDevice) override;

        void getRenderResolution(uint32_t* x, uint32_t* y);

        static glm::mat4 toMat4(vr::HmdMatrix34_t mat);
        static glm::mat4 toMat4(vr::HmdMatrix44_t mat);

        glm::mat4 getEyeViewMatrix(Eye eye) override;
        glm::mat4 getEyeProjectionMatrix(Eye eye, float near) override;
        glm::mat4 getEyeProjectionMatrix(Eye eye, float near, float far) override;

        void updateInput() override;
        bool getHandTransform(Hand hand, Transform& t) override;
        bool getHandVelocity(Hand hand, glm::vec3& velocity) override;
        glm::mat4 getHeadTransform(float predictionTime) override;
        glm::vec2 getLocomotionInput() override;

        Transform getHandBoneTransform(Hand hand, int boneIdx);

        bool getJumpInput() override;
        bool getSprintInput() override;
        bool hasFocus()
        {
            return hasInputFocus;
        }
        InputActionHandle getActionHandle(std::string actionPath) override;
        bool getActionHeld(InputActionHandle handle) override;
        bool getActionPressed(InputActionHandle handle) override;
        bool getActionReleased(InputActionHandle handle) override;
        void triggerHaptics(InputActionHandle handle, float timeFromNow, float duration, float frequency,
                            float amplitude) override;
        glm::vec2 getActionV2(InputActionHandle handle) override;
    };
}
