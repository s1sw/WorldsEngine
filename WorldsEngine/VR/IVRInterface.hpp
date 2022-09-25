#pragma once
#include <Core/Transform.hpp>
#include <glm/glm.hpp>
#include <string>
#include <vector>

extern "C"
{
    typedef struct VkPhysicalDevice_T* VkPhysicalDevice;
}

namespace worlds
{
    enum class VrApi
    {
        None,
        OpenXR,
        OpenVR
    };

    enum class TrackedObject
    {
        HMD,
        LeftHand,
        RightHand
    };

    enum class Hand
    {
        LeftHand,
        RightHand
    };

    enum class Eye
    {
        LeftEye,
        RightEye
    };

    typedef uint64_t InputActionHandle;

    struct HiddenMeshData
    {
        std::vector<glm::vec2> verts;
        uint32_t triangleCount;
    };

    class IVRInterface
    {
    public:
        virtual void getRenderResolution(uint32_t* x, uint32_t* y) = 0;
        virtual void updateInput() = 0;
        virtual float getPredictAmount() = 0;
        virtual void waitGetPoses() = 0;

        virtual Transform getHandBoneTransform(Hand hand, int boneIdx) = 0;

        virtual bool getHiddenMeshData(Eye eye, HiddenMeshData& hmd) = 0;
        virtual glm::mat4 getEyeViewMatrix(Eye eye) = 0;
        virtual glm::mat4 getEyeProjectionMatrix(Eye eye, float near) = 0;
        virtual glm::mat4 getEyeProjectionMatrix(Eye eye, float near, float far) = 0;

        virtual glm::mat4 getHeadTransform(float predictionTime = 0.0f) = 0;
        virtual bool getHandTransform(Hand hand, Transform& t) = 0;
        virtual bool getHandVelocity(Hand hand, glm::vec3& velocity) = 0;

        virtual InputActionHandle getActionHandle(std::string actionPath) = 0;
        virtual bool getActionHeld(InputActionHandle handle) = 0;
        virtual bool getActionPressed(InputActionHandle handle) = 0;
        virtual bool getActionReleased(InputActionHandle handle) = 0;
        virtual void triggerHaptics(InputActionHandle handle, float timeFromNow, float duration, float frequency,
                                    float amplitude) = 0;
        virtual glm::vec2 getActionV2(InputActionHandle handle) = 0;

        virtual std::vector<std::string> getVulkanInstanceExtensions() = 0;
        virtual std::vector<std::string> getVulkanDeviceExtensions(VkPhysicalDevice physDevice) = 0;

        virtual ~IVRInterface()
        {
        }
    };
}
