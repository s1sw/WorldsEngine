#pragma once
#include <VR/IVRInterface.hpp>

#define XR_DEFINE_HANDLE(object) typedef struct object##_T* object;
XR_DEFINE_HANDLE(XrInstance)
XR_DEFINE_HANDLE(XrSession)
XR_DEFINE_HANDLE(XrSpace)
XR_DEFINE_HANDLE(XrSwapchain)
#undef XR_DEFINE_HANDLE
#define XR_DEFINE_ATOM(object) typedef uint64_t object;
XR_DEFINE_ATOM(XrSystemId)
#undef XR_DEFINE_ATOM

namespace worlds
{
    struct EngineInterfaces;

    class OpenXRInterface : public IVRInterface
    {
    public:
        OpenXRInterface(const EngineInterfaces& interfaces);
        ~OpenXRInterface() override;
        void init() override;
        bool hasFocus() override;

        // Eye related functions
        bool getHiddenMeshData(Eye eye, HiddenMeshData& hmd) override;
        glm::mat4 getEyeViewMatrix(Eye eye) override;
        glm::mat4 getEyeProjectionMatrix(Eye eye, float near) override;
        glm::mat4 getEyeProjectionMatrix(Eye eye, float near, float far) override;

        // Head/hand related functions
        glm::mat4 getHeadTransform(float predictionTime = 0.0f) override;
        bool getHandTransform(Hand hand, Transform& t) override;
        bool getHandVelocity(Hand hand, glm::vec3& velocity) override;
        Transform getHandBoneTransform(Hand hand, int boneIdx) override;

        // Input related functions
        void updateInput() override;
        InputActionHandle getActionHandle(std::string actionPath) override;
        bool getActionHeld(InputActionHandle handle) override;
        bool getActionPressed(InputActionHandle handle) override;
        bool getActionReleased(InputActionHandle handle) override;
        void triggerHaptics(InputActionHandle handle, float timeFromNow, float duration, float frequency,
                                    float amplitude) override;
        glm::vec2 getActionV2(InputActionHandle handle) override;

        // Render related functions
        std::vector<std::string> getVulkanInstanceExtensions() override;
        std::vector<std::string> getVulkanDeviceExtensions(VkPhysicalDevice physDevice) override;
        void getRenderResolution(uint32_t* x, uint32_t* y) override;
        float getPredictAmount() override;
        void preSubmit() override;
        void submit(VRSubmitInfo submitInfo) override;
        void waitGetPoses() override;
    private:
        class OpenXRSwapchain;
        const EngineInterfaces& interfaces;
        XrInstance instance;
        XrSystemId systemId;
        XrSession session;
        XrSpace stageReferenceSpace;
        XrSpace viewReferenceSpace;
        std::vector<OpenXRSwapchain> swapchains;
    };
}