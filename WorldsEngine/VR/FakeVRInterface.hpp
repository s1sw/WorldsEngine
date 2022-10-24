#include "IVRInterface.hpp"

namespace worlds
{
    class FakeVRInterface : public IVRInterface
    {
    public:
        void init() override;
        bool hasFocus() override;

        bool getHiddenMeshData(Eye eye, HiddenMeshData& hmd) override;
        glm::mat4 getEyeViewMatrix(Eye eye) override;
        glm::mat4 getEyeProjectionMatrix(Eye eye, float near) override;
        glm::mat4 getEyeProjectionMatrix(Eye eye, float near, float far) override;

        glm::mat4 getHeadTransform(float predictionTime) override;
        bool getHandTransform(Hand hand, Transform& t) override;
        bool getHandVelocity(Hand hand, glm::vec3& velocity) override;
        Transform getHandBoneTransform(Hand hand, int boneIdx);

        void updateInput() override;
        InputActionHandle getActionHandle(std::string actionPath) override;
        bool getActionHeld(InputActionHandle handle) override;
        bool getActionPressed(InputActionHandle handle) override;
        bool getActionReleased(InputActionHandle handle) override;
        void triggerHaptics(InputActionHandle handle, float timeFromNow, float duration, float frequency,
                            float amplitude) override;
        glm::vec2 getActionV2(InputActionHandle handle) override;

        std::vector<std::string> getVulkanInstanceExtensions() override;
        std::vector<std::string> getVulkanDeviceExtensions(VkPhysicalDevice physDevice) override;
        void getRenderResolution(uint32_t* x, uint32_t* y) override;
        float getPredictAmount() override;
        void preSubmit() override;
        void submit(VRSubmitInfo submitInfo) override;
        void waitGetPoses() override;
    };
}