#include "OpenXRInterface.hpp"
#include <Core/Log.hpp>
#include <Core/Fatal.hpp>
#include <string.h>
#include <Core/Fatal.hpp>
#include <Render/RenderInternal.hpp>
#include <Core/Engine.hpp>

#define XR_USE_GRAPHICS_API_VULKAN
#include <vulkan/vulkan.h>
#include <R2/VK.hpp>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <glm/gtc/type_ptr.hpp>

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

    class OpenXRInterface::OpenXRSwapchain
    {
        XrSwapchain swapchain;
    public:
        std::vector<XrSwapchainImageVulkanKHR> images;
        OpenXRSwapchain(XrSwapchain swapchain)
            : swapchain(swapchain)
        {
            // Enumerate swapchain images
            uint32_t imageCount;
            XRCHECK(xrEnumerateSwapchainImages(swapchain, 0, &imageCount, nullptr));

            images.resize(imageCount);

            XRCHECK(xrEnumerateSwapchainImages(swapchain, imageCount, &imageCount,
                    (XrSwapchainImageBaseHeader*)(images.data())));
        }
    };

    OpenXRInterface::OpenXRInterface(const EngineInterfaces& interfaces)
        : interfaces(interfaces)
    {
    }

    void OpenXRInterface::init()
    {
        XrInstanceCreateInfo instanceCreateInfo { XR_TYPE_INSTANCE_CREATE_INFO };

        // Create the instance
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

        // Get the XR system
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

        // Set up the Vulkan graphics binding
        const R2::VK::Handles* handles = renderer->getCore()->GetHandles();
        XrGraphicsBindingVulkanKHR graphicsBinding{ XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR };
        graphicsBinding.instance = handles->Instance;
        graphicsBinding.device = handles->Device;
        graphicsBinding.physicalDevice = handles->PhysicalDevice;
        graphicsBinding.queueFamilyIndex = handles->Queues.GraphicsFamilyIndex;
        graphicsBinding.queueIndex = 0;

        // Create the XR session
        XrSessionCreateInfo sessionCreateInfo{ XR_TYPE_SESSION_CREATE_INFO };
        sessionCreateInfo.systemId = systemId;
        sessionCreateInfo.next = &graphicsBinding;

        XRCHECK(xrCreateSession(instance, &sessionCreateInfo, &session));

        XrReferenceSpaceCreateInfo referenceSpaceCreateInfo { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;

        XRCHECK(xrCreateReferenceSpace(session, &referenceSpaceCreateInfo, &stageReferenceSpace));

        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;

        XRCHECK(xrCreateReferenceSpace(session, &referenceSpaceCreateInfo, &viewReferenceSpace));

        XrViewConfigurationProperties viewConfigProps{};

        XRCHECK(xrGetViewConfigurationProperties(instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, &viewConfigProps));

        std::vector<XrViewConfigurationView> viewConfigViews;

        uint32_t viewCount;
        XRCHECK(xrEnumerateViewConfigurationViews(
                instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                0, &viewCount, nullptr));

        if (viewCount == 0)
        {
            fatalErr("Nooo views?! <:(");
        }

        viewConfigViews.resize(viewCount);

        XRCHECK(xrEnumerateViewConfigurationViews(
                instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                (uint32_t)viewConfigViews.size(), &viewCount, viewConfigViews.data()));

        std::vector<int64_t> swapchainFormats;
        uint32_t numSwapchainFormats;
        XRCHECK(xrEnumerateSwapchainFormats(session, 0, &numSwapchainFormats, nullptr));
        swapchainFormats.resize(numSwapchainFormats);
        XRCHECK(xrEnumerateSwapchainFormats(session, numSwapchainFormats, &numSwapchainFormats,
                                            swapchainFormats.data()));

        bool foundFormat = false;
        for (int64_t format : swapchainFormats)
        {
            if (format == VK_FORMAT_R8G8B8A8_SRGB)
            {
                foundFormat = true;
                break;
            }
        }

        if (!foundFormat)
        {
            fatalErr("Couldn't find correct OpenXR swapchain format");
        }

        for (int i = 0; i < viewCount; i++)
        {
            const XrViewConfigurationView& viewConfigView = viewConfigViews[i];

            XrSwapchainCreateInfo swapchainCreateInfo{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
            swapchainCreateInfo.arraySize = 1;
            swapchainCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
            swapchainCreateInfo.width = viewConfigView.recommendedImageRectWidth;
            swapchainCreateInfo.height = viewConfigView.recommendedImageRectHeight;
            swapchainCreateInfo.mipCount = 1;
            swapchainCreateInfo.faceCount = 1;
            swapchainCreateInfo.sampleCount = 1;
            swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;

            XrSwapchain swapchain;
            XRCHECK(xrCreateSwapchain(session, &swapchainCreateInfo, &swapchain));
            swapchains.emplace_back(swapchain);
        }
    }

    OpenXRInterface::~OpenXRInterface()
    {
        XRCHECK(xrDestroySpace(stageReferenceSpace));
        XRCHECK(xrDestroySession(session));
        XRCHECK(xrDestroyInstance(instance));
    }

    bool OpenXRInterface::hasFocus()
    {
        return true;
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
        XrSpaceLocation spaceLocation { XR_TYPE_SPACE_LOCATION };
        XRCHECK(xrLocateSpace(viewReferenceSpace, stageReferenceSpace, 0, &spaceLocation));
        Transform t{glm::make_vec3(&spaceLocation.pose.position.x), glm::make_quat(&spaceLocation.pose.orientation.x));
        return t.getMatrix();
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
        // we're gonna poll for OpenXR events here
        // YOLO!

        XrEventDataBuffer event{};
        while (true)
        {
            event = XrEventDataBuffer{ XR_TYPE_EVENT_DATA_BUFFER };
            XrResult pollResult = xrPollEvent(instance, &event);

            if (pollResult == XR_SUCCESS)
            {
                // TODO
                if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED)
                {
                    auto* evt = (XrEventDataSessionStateChanged*)&event;
                    if (evt->state == XR_SESSION_STATE_READY)
                    {
                        XrSessionBeginInfo beginInfo{ XR_TYPE_SESSION_BEGIN_INFO };
                        beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                        XRCHECK(xrBeginSession(session, &beginInfo));
                    }
                    else
                    {
                        XRCHECK(xrEndSession(session));
                        fatalErr("Note to self: finish OpenXR session state changes");
                    }
                }
            }
            else if (pollResult == XR_EVENT_UNAVAILABLE)
            {
                break;
            }
            else
            {
                fatalErr("xrPollEvent failed!");
            }
        }
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
        std::vector<XrViewConfigurationView> viewConfigViews;

        uint32_t viewCount;
        XRCHECK(xrEnumerateViewConfigurationViews(
                instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                0, &viewCount, nullptr));

        if (viewCount == 0)
        {
            fatalErr("Nooo views?! <:(");
        }

        viewConfigViews.resize(viewCount);

        XRCHECK(xrEnumerateViewConfigurationViews(
                instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                (uint32_t)viewConfigViews.size(), &viewCount, viewConfigViews.data()));

        *x = viewConfigViews[0].recommendedImageRectWidth;
        *y = viewConfigViews[0].recommendedImageRectHeight;
    }

    float OpenXRInterface::getPredictAmount()
    {
        return 0;
    }

    void OpenXRInterface::preSubmit()
    {
        XrFrameBeginInfo frameBeginInfo{ XR_TYPE_FRAME_BEGIN_INFO };
        XRCHECK(xrBeginFrame(session, &frameBeginInfo));
    }

    void OpenXRInterface::submit(VRSubmitInfo submitInfo)
    {
    }

    void OpenXRInterface::waitGetPoses()
    {
        XrFrameWaitInfo waitInfo{ XR_TYPE_FRAME_WAIT_INFO };
        XrFrameState frameState{ XR_TYPE_FRAME_STATE };
        XRCHECK(xrWaitFrame(session, &waitInfo, &frameState));
    }
}