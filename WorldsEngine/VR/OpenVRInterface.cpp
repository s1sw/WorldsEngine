#include "OpenVRInterface.hpp"
#include <Core/Console.hpp>
#include <Core/Fatal.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <R2/VK.hpp>
#include <SDL_filesystem.h>
#include <Util/MatUtil.hpp>
#include "openvr.h"

namespace worlds
{
#define VRCHECK(val) checkErr(val, __FILE__, __LINE__)
    vr::EVREye convEye(Eye e)
    {
        if (e == Eye::LeftEye)
            return vr::EVREye::Eye_Left;
        else
            return vr::EVREye::Eye_Right;
    }

    void checkErr(vr::EVRInputError err, const char* file, int line)
    {
        if (err != vr::VRInputError_None)
        {
            logErr("VR Input Error: %i (%s:%i)", (int)err, file, line);
        }
    }

    void OpenVRInterface::init()
    {
        vr::EVRInitError eError = vr::VRInitError_None;
        system = vr::VR_Init(&eError, vr::VRApplication_Scene);

        if (eError != vr::VRInitError_None)
        {
            std::string errMsg =
                std::string{"Unable to init VR runtime: "} + vr::VR_GetVRInitErrorAsEnglishDescription(eError);
            fatalErr(errMsg.c_str());
        }

        logMsg("Initializing OpenVR");

        const char* basePath = SDL_GetBasePath();

        std::string actionsPathStr = basePath;
        actionsPathStr += "actions.json";

        const char* actionsPath = actionsPathStr.c_str();
        logMsg("Using %s as actionsPath", actionsPath);
        auto vrInput = vr::VRInput();
        vrInput->SetActionManifestPath(actionsPath);
        VRCHECK(vrInput->GetActionHandle("/actions/main/in/LeftHand", &leftHand));
        VRCHECK(vrInput->GetActionHandle("/actions/main/in/RightHand", &rightHand));

        VRCHECK(vrInput->GetActionHandle("/actions/main/in/LeftHand_Anim", &leftHandSkeletal));
        VRCHECK(vrInput->GetActionHandle("/actions/main/in/RightHand_Anim", &rightHandSkeletal));

        updateInput();

        // VRCHECK(vrInput->GetBoneCount(leftHandSkeletal, &handBoneCount));
        handBoneCount = 31;
        rhandBoneArray = (vr::VRBoneTransform_t*)malloc(sizeof(vr::VRBoneTransform_t) * handBoneCount);
        lhandBoneArray = (vr::VRBoneTransform_t*)malloc(sizeof(vr::VRBoneTransform_t) * handBoneCount);

        g_console->registerCommand(
            [&](const char*) {
                for (uint32_t i = 0; i < handBoneCount; i++)
                {
                    char buf[vr::k_unMaxBoneNameLength];
                    vr::VRInput()->GetBoneName(leftHandSkeletal, i, buf, vr::k_unMaxBoneNameLength);
                    logMsg("bone %u: %s", i, buf);
                }
            },
            "printbones", "printbones");

        VRCHECK(vrInput->GetActionSetHandle("/actions/main", &actionSet));

        vr::VRCompositor()->SetExplicitTimingMode(
            vr::VRCompositorTimingMode_Explicit_RuntimePerformsPostPresentHandoff);
    }

    std::vector<std::string> OpenVRInterface::getVulkanInstanceExtensions()
    {
        uint32_t extCharCount = vr::VRCompositor()->GetVulkanInstanceExtensionsRequired(nullptr, 0);

        std::string str((size_t)extCharCount, 'Z');

        vr::VRCompositor()->GetVulkanInstanceExtensionsRequired(str.data(), (uint32_t)str.size());

        std::vector<std::string> extensions;

        std::istringstream stringStream(str.substr(0, extCharCount));
        std::string currExt;
        while (std::getline(stringStream, currExt, ' '))
        {
            extensions.push_back(currExt);
        }

        return extensions;
    }

    std::vector<std::string> OpenVRInterface::getVulkanDeviceExtensions(VkPhysicalDevice physDevice)
    {
        uint32_t extCharCount = vr::VRCompositor()->GetVulkanDeviceExtensionsRequired(physDevice, nullptr, 0);

        std::string str((size_t)extCharCount, 'Z');

        vr::VRCompositor()->GetVulkanDeviceExtensionsRequired(physDevice, str.data(), (uint32_t)str.size());

        std::vector<std::string> extensions;

        std::istringstream stringStream(str);
        std::string currExt;
        while (std::getline(stringStream, currExt, ' '))
        {
            extensions.push_back(currExt);
        }

        return extensions;
    }

    void composeProjection(float fLeft, float fRight, float fTop, float fBottom, float zNear, glm::mat4& p)
    {
        float idx = 1.0f / (fRight - fLeft);
        float idy = 1.0f / (fBottom - fTop);
        float sx = fRight + fLeft;
        float sy = fBottom + fTop;

        p[0][0] = 2 * idx;
        p[1][0] = 0;
        p[2][0] = sx * idx;
        p[3][0] = 0;
        p[0][1] = 0;
        p[1][1] = 2 * idy;
        p[2][1] = sy * idy;
        p[3][1] = 0;
        p[0][2] = 0;
        p[1][2] = 0;
        p[2][2] = 0.0f;
        p[3][2] = zNear;
        p[0][3] = 0;
        p[1][3] = 0;
        p[2][3] = -1.0f;
        p[3][3] = 0;
    }

    void OpenVRInterface::getRenderResolution(uint32_t* x, uint32_t* y)
    {
        vr::VRSystem()->GetRecommendedRenderTargetSize(x, y);
    }

    float OpenVRInterface::getPredictAmount()
    {
        auto vrSys = vr::VRSystem();

        float secondsSinceLastVsync;
        vrSys->GetTimeSinceLastVsync(&secondsSinceLastVsync, NULL);

        float hmdFrequency =
            vrSys->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_DisplayFrequency_Float);

        float frameDuration = 1.f / hmdFrequency;
        float vsyncToPhotons = vrSys->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd,
                                                                    vr::Prop_SecondsFromVsyncToPhotons_Float);

        return frameDuration - secondsSinceLastVsync + vsyncToPhotons;
    }

    glm::mat4 OpenVRInterface::toMat4(vr::HmdMatrix34_t mat)
    {
        return glm::mat4(mat.m[0][0], mat.m[1][0], mat.m[2][0], 0.0f, mat.m[0][1], mat.m[1][1], mat.m[2][1], 0.0f,
                         mat.m[0][2], mat.m[1][2], mat.m[2][2], 0.0f, mat.m[0][3], mat.m[1][3], mat.m[2][3], 1.0f);
    }

    glm::mat4 OpenVRInterface::toMat4(vr::HmdMatrix44_t mat)
    {
        return glm::mat4(mat.m[0][0], mat.m[1][0], mat.m[2][0], mat.m[3][0], mat.m[0][1], mat.m[1][1], mat.m[2][1],
                         mat.m[3][1], mat.m[0][2], mat.m[1][2], mat.m[2][2], mat.m[3][2], mat.m[0][3], mat.m[1][3],
                         mat.m[2][3], mat.m[3][3]);
    }

    bool OpenVRInterface::getHiddenMeshData(Eye eye, HiddenMeshData& hmd)
    {
        vr::HiddenAreaMesh_t hiddenMesh = system->GetHiddenAreaMesh(convEye(eye));

        if (hiddenMesh.pVertexData == nullptr || hiddenMesh.unTriangleCount == 0)
        {
            return false;
        }

        hmd.triangleCount = hiddenMesh.unTriangleCount;
        hmd.verts.resize(hmd.triangleCount * 3);
        memcpy(hmd.verts.data(), hiddenMesh.pVertexData, hmd.triangleCount * 3 * sizeof(glm::vec2));

        return true;
    }

    glm::mat4 OpenVRInterface::getEyeViewMatrix(Eye eye)
    {
        return toMat4(system->GetEyeToHeadTransform(convEye(eye)));
    }

    glm::mat4 OpenVRInterface::getEyeProjectionMatrix(Eye eye, float near)
    {
        float left, right, top, bottom;
        system->GetProjectionRaw(convEye(eye), &left, &right, &top, &bottom);

        glm::mat4 m;

        composeProjection(left, right, top, bottom, near, m);
        return m;
    }

    glm::mat4 OpenVRInterface::getEyeProjectionMatrix(Eye eye, float near, float far)
    {
        return toMat4(system->GetProjectionMatrix(convEye(eye), near, far));
    }

    void OpenVRInterface::updateInput()
    {
        vr::VRActiveActionSet_t activeActionSet{.ulActionSet = actionSet};
        vr::VRInput()->UpdateActionState(&activeActionSet, sizeof(activeActionSet), 1);

        vr::VREvent_t vrEvent;
        while (vr::VRSystem()->PollNextEvent(&vrEvent, sizeof(vrEvent)))
        {
            switch (vrEvent.eventType)
            {
            case vr::EVREventType::VREvent_InputFocusCaptured:
                hasInputFocus = true;
                break;
            case vr::EVREventType::VREvent_InputFocusReleased:
                hasInputFocus = false;
                break;
            }
        }
    }

    bool OpenVRInterface::getHandTransform(Hand hand, Transform& t)
    {
        vr::InputPoseActionData_t pose;

        auto retVal = vr::VRInput()->GetPoseActionDataForNextFrame(hand == Hand::LeftHand ? leftHand : rightHand,
                                                                   vr::TrackingUniverseStanding, &pose, sizeof(pose),
                                                                   vr::k_ulInvalidInputValueHandle);

        if (retVal != vr::VRInputError_None)
            return false;

        vr::InputSkeletalActionData_t skeletalActionData;

        auto skeletalAction = hand == Hand::LeftHand ? leftHandSkeletal : rightHandSkeletal;

        retVal = vr::VRInput()->GetSkeletalActionData(skeletalAction, &skeletalActionData, sizeof(skeletalActionData));

        vr::VRBoneTransform_t* boneArray = hand == Hand::LeftHand ? lhandBoneArray : rhandBoneArray;

        glm::mat4 matrix = toMat4(pose.pose.mDeviceToAbsoluteTracking);

        t.rotation = getMatrixRotation(matrix);
        t.position = getMatrixTranslation(matrix);

        vr::VRInput()->GetSkeletalBoneData(skeletalAction, vr::VRSkeletalTransformSpace_Parent,
                                           vr::VRSkeletalMotionRange_WithoutController, boneArray, handBoneCount);

        if (glm::any(glm::isnan(t.position)) || glm::any(glm::isnan(t.rotation)))
            return false;

        return true;
    }

    bool OpenVRInterface::getHandVelocity(Hand hand, glm::vec3& vel)
    {
        vr::InputPoseActionData_t pose;

        auto retVal = vr::VRInput()->GetPoseActionDataForNextFrame(hand == Hand::LeftHand ? leftHand : rightHand,
                                                                   vr::TrackingUniverseStanding, &pose, sizeof(pose),
                                                                   vr::k_ulInvalidInputValueHandle);

        if (retVal != vr::VRInputError_None)
            return false;

        vel = glm::make_vec3(pose.pose.vVelocity.v);
        return true;
    }

    glm::mat4 OpenVRInterface::getHeadTransform(float predictionTime)
    {
        vr::TrackedDevicePose_t hmdPose;
        system->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, predictionTime, &hmdPose, 1);
        return toMat4(hmdPose.mDeviceToAbsoluteTracking);
    }

    Transform OpenVRInterface::getHandBoneTransform(Hand hand, int boneIdx)
    {
        vr::VRBoneTransform_t* arr = hand == Hand::LeftHand ? lhandBoneArray : rhandBoneArray;
        vr::VRBoneTransform_t bt = arr[boneIdx];

        return Transform{glm::vec3{bt.position.v[0], bt.position.v[1], bt.position.v[2]},
                         glm::quat{bt.orientation.w, bt.orientation.x, bt.orientation.y, bt.orientation.z}};
    }

    void OpenVRInterface::waitGetPoses()
    {
        vr::VRCompositor()->WaitGetPoses(nullptr, 0, nullptr, 0);
    }

    const char* inputErrorStrings[] = {"None",
                                       "NameNotFound",
                                       "WrongType",
                                       "InvalidHandle",
                                       "InvalidParam",
                                       "NoSteam",
                                       "MaxCapacityReached",
                                       "IPCError",
                                       "NoActiveActionSet",
                                       "InvalidDevice",
                                       "InvalidSkeleton",
                                       "InvalidBoneCount",
                                       "InvalidCompressedData",
                                       "NoData",
                                       "BufferTooSmall",
                                       "MismatchedActionManifest",
                                       "MissingSkeletonData",
                                       "InvalidBoneIndex"};

    InputActionHandle OpenVRInterface::getActionHandle(std::string actionPath)
    {
        vr::VRActionHandle_t handle = UINT64_MAX;
        vr::EVRInputError err = vr::VRInput()->GetActionHandle(actionPath.c_str(), &handle);

        if (err != vr::VRInputError_None)
        {
            logErr("Failed to get action %s: %s", actionPath.c_str(), inputErrorStrings);
        }

        return handle;
    }

    bool OpenVRInterface::getActionHeld(InputActionHandle handle)
    {
        vr::InputDigitalActionData_t data;
        vr::VRInput()->GetDigitalActionData(handle, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);

        return data.bState;
    }

    bool OpenVRInterface::getActionPressed(InputActionHandle handle)
    {
        vr::InputDigitalActionData_t data;
        vr::VRInput()->GetDigitalActionData(handle, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);
        return data.bState && data.bChanged;
    }

    bool OpenVRInterface::getActionReleased(InputActionHandle handle)
    {
        vr::InputDigitalActionData_t data;
        vr::VRInput()->GetDigitalActionData(handle, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);

        return !data.bState && data.bChanged;
    }

    void OpenVRInterface::triggerHaptics(InputActionHandle handle, float timeFromNow, float duration, float frequency,
                                         float amplitude)
    {
        vr::VRInput()->TriggerHapticVibrationAction(handle, timeFromNow, duration, frequency, amplitude,
                                                    vr::k_ulInvalidActionHandle);
    }

    glm::vec2 OpenVRInterface::getActionV2(InputActionHandle handle)
    {
        vr::InputAnalogActionData_t data;
        vr::VRInput()->GetAnalogActionData(handle, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);

        return glm::vec2{data.x, data.y};
    }

    vr::HmdMatrix34_t fromMat4(glm::mat4 mat)
    {
        vr::HmdMatrix34_t r{};

        for (int x = 0; x < 3; x++)
            for (int y = 0; y < 4; y++)
            {
                r.m[x][y] = mat[y][x];
            }

        return r;
    }

    void OpenVRInterface::submitExplicitTimingData()
    {
        vr::VRCompositor()->SubmitExplicitTimingData();
    }

    void OpenVRInterface::submit(VRSubmitInfo submitInfo)
    {
        vr::VRCompositor()->PostPresentHandoff();
        const R2::VK::Handles* handles = submitInfo.vkHandles;

        VkImage leftNativeHandle = submitInfo.leftEye->GetNativeHandle();
        VkImage rightNativeHandle = submitInfo.rightEye->GetNativeHandle();

        vr::VRVulkanTextureData_t leftVKTexData{};
        leftVKTexData.m_nImage = (uint64_t)leftNativeHandle;
        leftVKTexData.m_pDevice = handles->Device;
        leftVKTexData.m_pPhysicalDevice = handles->PhysicalDevice;
        leftVKTexData.m_pInstance = handles->Instance;
        leftVKTexData.m_pQueue = handles->Queues.Graphics;
        leftVKTexData.m_nQueueFamilyIndex = handles->Queues.GraphicsFamilyIndex;
        leftVKTexData.m_nWidth = submitInfo.leftEye->GetWidth();
        leftVKTexData.m_nHeight = submitInfo.leftEye->GetHeight();
        leftVKTexData.m_nFormat = (uint32_t)submitInfo.leftEye->GetFormat();
        leftVKTexData.m_nSampleCount = 1;

        vr::VRVulkanTextureData_t rightVKTexData{};
        rightVKTexData.m_nImage = (uint64_t)rightNativeHandle;
        rightVKTexData.m_pDevice = handles->Device;
        rightVKTexData.m_pPhysicalDevice = handles->PhysicalDevice;
        rightVKTexData.m_pInstance = handles->Instance;
        rightVKTexData.m_pQueue = handles->Queues.Graphics;
        rightVKTexData.m_nQueueFamilyIndex = handles->Queues.GraphicsFamilyIndex;
        rightVKTexData.m_nWidth = submitInfo.rightEye->GetWidth();
        rightVKTexData.m_nHeight = submitInfo.rightEye->GetHeight();
        rightVKTexData.m_nFormat = (uint32_t)submitInfo.rightEye->GetFormat();
        rightVKTexData.m_nSampleCount = 1;

        vr::VRTextureWithPose_t leftTex{};
        leftTex.handle = &leftVKTexData;
        leftTex.eColorSpace = vr::ColorSpace_Gamma;
        leftTex.eType = vr::TextureType_Vulkan;
        leftTex.mDeviceToAbsoluteTracking = fromMat4(submitInfo.renderPose);

        vr::VRCompositor()->Submit(vr::Eye_Left, &leftTex, nullptr, vr::Submit_TextureWithPose);

        vr::VRTextureWithPose_t rightTex{};
        rightTex.handle = &rightVKTexData;
        rightTex.eColorSpace = vr::ColorSpace_Gamma;
        rightTex.eType = vr::TextureType_Vulkan;
        rightTex.mDeviceToAbsoluteTracking = fromMat4(submitInfo.renderPose);

        vr::VRCompositor()->Submit(vr::Eye_Right, &rightTex, nullptr, vr::Submit_TextureWithPose);
    }
}
