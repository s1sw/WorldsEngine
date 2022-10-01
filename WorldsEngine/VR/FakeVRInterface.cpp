#include "FakeVRInterface.hpp"
#include <Core/Console.hpp>
#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <R2/VK.hpp>

namespace worlds
{
    void FakeVRInterface::init()
    {
        logMsg("FakeVR initialised!! ;)");
    }

    bool FakeVRInterface::hasFocus()
    {
        return true;
    }

    std::vector<std::string> FakeVRInterface::getVulkanInstanceExtensions()
    {
        return {};
    }

    std::vector<std::string> FakeVRInterface::getVulkanDeviceExtensions(VkPhysicalDevice physDevice)
    {
        return {};
    }

    void FakeVRInterface::getRenderResolution(uint32_t* x, uint32_t* y)
    {
        if (windowSize.x == 0 || windowSize.y == 0)
        {
            *x = 1600;
            *y = 900;
            return;
        }

        *x = windowSize.x;
        *y = windowSize.y;
    }

    float FakeVRInterface::getPredictAmount()
    {
        return 0;
    }

    bool FakeVRInterface::getHiddenMeshData(Eye eye, HiddenMeshData& hmd)
    {
        return false;
    }

    glm::mat4 FakeVRInterface::getEyeViewMatrix(Eye eye)
    {
        return glm::mat4{ 1.0f };
    }

    glm::mat4 FakeVRInterface::getEyeProjectionMatrix(Eye eye, float near)
    {
        return glm::infinitePerspective(glm::radians(70.0f), (float)windowSize.x / (float)windowSize.y, near);
    }

    glm::mat4 FakeVRInterface::getEyeProjectionMatrix(Eye eye, float near, float far)
    {
        return glm::mat4{1.0f};
    }

    void FakeVRInterface::updateInput()
    {
        if (ImGui::Begin("Fake VR"))
        {
        }
        ImGui::End();
    }

    bool FakeVRInterface::getHandTransform(Hand hand, Transform& t)
    {
        return false;
    }

    bool FakeVRInterface::getHandVelocity(Hand hand, glm::vec3& vel)
    {
        return false;
    }

    glm::mat4 FakeVRInterface::getHeadTransform(float predictionTime)
    {
        return glm::mat4{1.0f};
    }

    Transform FakeVRInterface::getHandBoneTransform(Hand hand, int boneIdx)
    {
        return Transform{};
    }

    void FakeVRInterface::waitGetPoses()
    {
    }

    InputActionHandle FakeVRInterface::getActionHandle(std::string actionPath)
    {
        return 0;
    }

    bool FakeVRInterface::getActionHeld(InputActionHandle handle)
    {
        return false;
    }

    bool FakeVRInterface::getActionPressed(InputActionHandle handle)
    {
        return false;
    }

    bool FakeVRInterface::getActionReleased(InputActionHandle handle)
    {
        return false;
    }

    void FakeVRInterface::triggerHaptics(InputActionHandle handle, float timeFromNow, float duration, float frequency,
                                         float amplitude)
    {
    }

    glm::vec2 FakeVRInterface::getActionV2(InputActionHandle handle)
    {
        return glm::vec2{0.0f, 0.0f};
    }

    void FakeVRInterface::submitExplicitTimingData()
    {
    }

    void FakeVRInterface::submit(VRSubmitInfo submitInfo)
    {
    }
}
