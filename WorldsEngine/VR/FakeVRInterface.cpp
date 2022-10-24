#include "FakeVRInterface.hpp"
#include <Core/Console.hpp>
#include <Editor/GuiUtil.hpp>
#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <robin_hood.h>
#include <R2/VK.hpp>

namespace worlds
{
    Transform hmdTransform{glm::vec3(0.0f, 1.6f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f)};
    Transform leftHandTransform{glm::vec3(-0.2f, 1.2f, -0.3f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f)};
    Transform rightHandTransform{glm::vec3(0.2f, 1.2f, -0.3f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f)};

    struct FakeVRAction
    {
        std::string path;
        bool value = false;
        bool valueLast = false;
        bool pressed = false;
        bool released = false;
        glm::vec2 v2Val;
    };

    std::vector<FakeVRAction> actions;
    robin_hood::unordered_flat_map<std::string, InputActionHandle> handles;

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
        Camera c{};
        c.near = near;
        return c.getProjectionMatrix((float)windowSize.x / (float)windowSize.y);
        //return glm::infinitePerspective(glm::radians(70.0f), (float)windowSize.x / (float)windowSize.y, near);
    }

    glm::mat4 FakeVRInterface::getEyeProjectionMatrix(Eye eye, float near, float far)
    {
        return glm::perspective(glm::radians(70.0f), (float)windowSize.x / (float)windowSize.y, near, far);
    }

    void FakeVRInterface::updateInput()
    {
        if (ImGui::Begin("Fake VR"))
        {
            if (ImGui::CollapsingHeader("Exact Controls"))
            {
                pushBoldFont();
                ImGui::Text("Headset");
                ImGui::PopFont();

                ImGui::DragFloat3("Position##HMD", glm::value_ptr(hmdTransform.position), 0.01f);

                glm::vec3 ea = glm::eulerAngles(hmdTransform.rotation);

                ImGui::DragFloat("Yaw##HMD", &ea.y);

                hmdTransform.rotation = glm::quat(ea);
                ImGui::Separator();

                pushBoldFont();
                ImGui::Text("Left Hand");
                ImGui::PopFont();

                ImGui::DragFloat3("Position##LH", glm::value_ptr(leftHandTransform.position), 0.01f);
                ImGui::Separator();

                pushBoldFont();
                ImGui::Text("Right Hand");
                ImGui::PopFont();

                ImGui::DragFloat3("Position##RH", glm::value_ptr(rightHandTransform.position), 0.01f);
                ImGui::Separator();
            }

            if (ImGui::CollapsingHeader("Input Actions"))
            {
                for (auto& action : actions)
                {
                    if (ImGui::TreeNode(action.path.c_str()))
                    {
                        ImGui::Checkbox("Value", &action.value);
                        ImGui::DragFloat2("V2 val", &action.v2Val.x);
                        ImGui::TreePop();
                    }
                }
            }
        }
        ImGui::End();

        for (auto& action : actions)
        {
            action.pressed = false;
            action.released = false;

            if (action.value && !action.valueLast)
            {
                action.pressed = true;
            }

            if (action.valueLast && !action.value)
            {
                action.released = true;
            }

            action.valueLast = action.value;
        }
    }

    bool FakeVRInterface::getHandTransform(Hand hand, Transform& t)
    {
        if (hand == Hand::LeftHand)
        {
            t = leftHandTransform;
        }
        else if (hand == Hand::RightHand)
        {
            t = rightHandTransform;
        }

        return true;
    }

    bool FakeVRInterface::getHandVelocity(Hand hand, glm::vec3& vel)
    {
        return false;
    }

    glm::mat4 FakeVRInterface::getHeadTransform(float predictionTime)
    {
        return hmdTransform.getMatrix();
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
        if (handles.contains(actionPath))
            return handles[actionPath];

        actions.emplace_back(actionPath);

        InputActionHandle handle = actions.size() - 1;
        handles.insert({ actionPath, handle });

        return handle;
    }

    bool FakeVRInterface::getActionHeld(InputActionHandle handle)
    {
        return actions[handle].value;
    }

    bool FakeVRInterface::getActionPressed(InputActionHandle handle)
    {
        return actions[handle].pressed;
    }

    bool FakeVRInterface::getActionReleased(InputActionHandle handle)
    {
        return actions[handle].pressed;
    }

    void FakeVRInterface::triggerHaptics(InputActionHandle handle, float timeFromNow, float duration, float frequency,
                                         float amplitude)
    {
    }

    glm::vec2 FakeVRInterface::getActionV2(InputActionHandle handle)
    {
        return actions[handle].v2Val;
    }

    void FakeVRInterface::preSubmit()
    {
    }

    void FakeVRInterface::submit(VRSubmitInfo submitInfo)
    {
    }
}
