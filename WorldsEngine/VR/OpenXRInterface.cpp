#include "OpenXRInterface.hpp"
#include <Core/Log.hpp>
#include <Core/Fatal.hpp>
#define XR_USE_GRAPHICS_API_VULKAN
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <R2/VK.hpp>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <string.h>
#include <Core/Fatal.hpp>
#include <Render/RenderInternal.hpp>
#include <Core/Engine.hpp>

namespace worlds
{
#define XRCHECK(expr) checkXrResult(expr, __FILE__, __LINE__)
    void checkXrResult(XrResult result, const char* file, int line)
    {
        if (!XR_SUCCEEDED(result))
        {
            logErr("OpenXR returned %i (file %s, line %i)", result, file, line);
        }
    }

    OpenXRInterface::OpenXRInterface(const EngineInterfaces& interfaces)
        : interfaces(interfaces)
    {
    }

    void OpenXRInterface::init()
    {
        XrInstanceCreateInfo instanceCreateInfo { XR_TYPE_INSTANCE_CREATE_INFO };

        XrApplicationInfo appInfo{};
        appInfo.apiVersion = XR_CURRENT_API_VERSION;
        memcpy(appInfo.applicationName, "WorldsEngine", sizeof("WorldsEngine"));
        memcpy(appInfo.engineName, "WorldsEngine", sizeof("WorldsEngine"));
        appInfo.engineVersion = 1;
        appInfo.applicationVersion = 1;

        instanceCreateInfo.applicationInfo = appInfo;

        std::vector<const char*> extensions;
        extensions.push_back(XR_KHR_VULKAN_ENABLE_EXTENSION_NAME);

        instanceCreateInfo.enabledExtensionCount = extensions.size();
        instanceCreateInfo.enabledExtensionNames = extensions.data();

        XRCHECK(xrCreateInstance(&instanceCreateInfo, &instance));

        XrSystemGetInfo systemGetInfo{ XR_TYPE_SYSTEM_GET_INFO };
        systemGetInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

        XrResult getSystemResult = xrGetSystem(instance, &systemGetInfo, &systemId);

        if (getSystemResult == XR_ERROR_FORM_FACTOR_UNAVAILABLE)
        {
            fatalErr("Couldn't find HMD!");
        }

        XrSystemProperties systemProperties{};
        XRCHECK(xrGetSystemProperties(instance, systemId, &systemProperties));

        logMsg("XR System name: %s (vendor ID %u)", systemProperties.systemName, systemProperties.vendorId);

        VKRenderer* renderer = (VKRenderer*)interfaces.renderer;

        const R2::VK::Handles* handles = renderer->getCore()->GetHandles();
        XrGraphicsBindingVulkanKHR graphicsBinding{ XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR };
        graphicsBinding.instance = handles->Instance;
        graphicsBinding.device = handles->Device;
        graphicsBinding.physicalDevice = handles->PhysicalDevice;
        graphicsBinding.queueFamilyIndex = handles->Queues.GraphicsFamilyIndex;
        graphicsBinding.queueIndex = 0;

        XrSessionCreateInfo sessionCreateInfo{ XR_TYPE_SESSION_CREATE_INFO };
        sessionCreateInfo.systemId = systemId;
        sessionCreateInfo.next = &graphicsBinding;

        XRCHECK(xrCreateSession(instance, &sessionCreateInfo, &session));

        XrReferenceSpaceCreateInfo referenceSpaceCreateInfo { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;

        XRCHECK(xrCreateReferenceSpace(session, &referenceSpaceCreateInfo, &stageReferenceSpace));

        XrViewConfigurationProperties viewConfigProps{};

        XRCHECK(xrGetViewConfigurationProperties(instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, &viewConfigProps));

        std::vector<XrViewConfigurationView> viewConfigViews;

        uint32_t viewCount;
        XRCHECK(xrEnumerateViewConfigurationViews(
                instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                0, &viewCount, nullptr));

        viewConfigViews.resize(viewCount);

        XRCHECK(xrEnumerateViewConfigurationViews(
                instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                (uint32_t)viewConfigViews.size(), &viewCount, viewConfigViews.data()));

    }

    OpenXRInterface::~OpenXRInterface()
    {
        XRCHECK(xrDestroySpace(stageReferenceSpace));
        XRCHECK(xrDestroySession(session));
        XRCHECK(xrDestroyInstance(instance));
    }

    bool OpenXRInterface::hasFocus()
    {
        return false;
    }

    bool OpenXRInterface::getHiddenMeshData(Eye eye, HiddenMeshData &hmd)
    {
        return false;
    }

    glm::mat4 OpenXRInterface::getEyeViewMatrix(Eye eye)
    {
        return glm::mat4();
    }

    glm::mat4 OpenXRInterface::getEyeProjectionMatrix(Eye eye, float near)
    {
        return glm::mat4();
    }

    glm::mat4 OpenXRInterface::getEyeProjectionMatrix(Eye eye, float near, float far)
    {
        return glm::mat4();
    }

    glm::mat4 OpenXRInterface::getHeadTransform(float predictionTime)
    {
        return glm::mat4();
    }

    bool OpenXRInterface::getHandTransform(Hand hand, Transform &t)
    {
        return false;
    }

    bool OpenXRInterface::getHandVelocity(Hand hand, glm::vec3 &velocity)
    {
        return false;
    }

    Transform OpenXRInterface::getHandBoneTransform(Hand hand, int boneIdx)
    {
        return Transform();
    }

    void OpenXRInterface::updateInput()
    {

    }

    InputActionHandle OpenXRInterface::getActionHandle(std::string actionPath)
    {
        return 0;
    }

    bool OpenXRInterface::getActionHeld(InputActionHandle handle)
    {
        return false;
    }

    bool OpenXRInterface::getActionPressed(InputActionHandle handle)
    {
        return false;
    }

    bool OpenXRInterface::getActionReleased(InputActionHandle handle)
    {
        return false;
    }

    void OpenXRInterface::triggerHaptics(InputActionHandle handle, float timeFromNow, float duration, float frequency,
                                         float amplitude)
    {

    }

    glm::vec2 OpenXRInterface::getActionV2(InputActionHandle handle)
    {
        return glm::vec2();
    }

    std::vector<std::string> OpenXRInterface::getVulkanInstanceExtensions()
    {
        return std::vector<std::string>();
    }

    std::vector<std::string> OpenXRInterface::getVulkanDeviceExtensions(VkPhysicalDevice physDevice)
    {
        return std::vector<std::string>();
    }

    void OpenXRInterface::getRenderResolution(uint32_t *x, uint32_t *y)
    {

    }

    float OpenXRInterface::getPredictAmount()
    {
        return 0;
    }

    void OpenXRInterface::submitExplicitTimingData()
    {

    }

    void OpenXRInterface::submit(VRSubmitInfo submitInfo)
    {

    }

    void OpenXRInterface::waitGetPoses()
    {

    }
}