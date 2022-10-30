#include "OpenXRInterface.hpp"
// EW EW EW
#include "../../R2/PrivateInclude/volk.h"
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
#include <Util/EnumUtil.hpp>
#include <IO/IOUtil.hpp>
#include <nlohmann/json.hpp>
#include <robin_hood.h>

using namespace R2;

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
    public:
        std::vector<XrSwapchainImageVulkanKHR> images;
        XrSwapchain swapchain;
        OpenXRSwapchain(XrSwapchain swapchain)
            : swapchain(swapchain)
        {
            // Enumerate swapchain images
            uint32_t imageCount;
            XRCHECK(xrEnumerateSwapchainImages(swapchain, 0, &imageCount, nullptr));

            images.resize(imageCount, { XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR });

            XRCHECK(xrEnumerateSwapchainImages(swapchain, imageCount, &imageCount,
                    (XrSwapchainImageBaseHeader*)(images.data())));
        }
    };


    struct ActionInternal
    {
        XrAction action;
        bool isPoseAction;
        XrSpace poseSpace;
        robin_hood::unordered_flat_map<XrPath, XrSpace> poseSubactionSpaces;
    };

    robin_hood::unordered_flat_map<uint64_t, ActionInternal> actionsInternal;

    class ActionSet
    {
    public:
        XrActionSet actionSet;
        std::string name;
        robin_hood::unordered_flat_map<std::string, uint64_t> actions;

        ActionSet(XrInstance instance, XrSession session, std::string name, nlohmann::json& value)
             : name(name)
             , actionSet(XR_NULL_HANDLE)
        {
            XrActionSetCreateInfo setCreateInfo{ XR_TYPE_ACTION_SET_CREATE_INFO };

            if (name.size() >= XR_MAX_ACTION_SET_NAME_SIZE)
            {
                logErr("Action set name %s is too long", name.c_str());
                return;
            }

            strcpy(setCreateInfo.actionSetName, name.c_str());
            // At some point, finish this so the action set name is localized
            strcpy(setCreateInfo.localizedActionSetName, name.c_str());
            setCreateInfo.priority = 0;
            XRCHECK(xrCreateActionSet(instance, &setCreateInfo, &actionSet));

            // Iterate over the actions and add them to the set
            for (auto& actionPair : value["actions"].items())
            {
                std::string actionName = actionPair.key();
                if (actionName.size() >= XR_MAX_ACTION_NAME_SIZE)
                {
                    logErr("Action name %s is too long!", actionName.c_str());
                    continue;
                }

                ActionInternal actionInternal{};
                XrActionCreateInfo actionCreateInfo{ XR_TYPE_ACTION_CREATE_INFO };
                strcpy(actionCreateInfo.actionName, actionName.c_str());
                strcpy(actionCreateInfo.localizedActionName, actionName.c_str());

                auto& actionTypeVal = actionPair.value()["type"];

                if (actionTypeVal == "boolean")
                {
                    actionCreateInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
                }
                else if (actionTypeVal == "float")
                {
                    actionCreateInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
                }
                else if (actionTypeVal == "pose")
                {
                    actionCreateInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
                    actionInternal.isPoseAction = true;
                }
                else if (actionTypeVal == "vibration_output")
                {
                    actionCreateInfo.actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT;
                }
                else
                {
                    logErr("Invalid action type %s", actionTypeVal.get<std::string>().c_str());
                    continue;
                }

                // There are a maximum of 4 possible actions, so just use an array for that
                XrPath subactions[4];
                if (actionPair.value().contains("subactions"))
                {
                    auto& subactionsVal = actionPair.value()["subactions"];
                    actionCreateInfo.countSubactionPaths = subactionsVal.size();
                    actionCreateInfo.subactionPaths = subactions;

                    for (int i = 0; i < subactionsVal.size(); i++)
                    {
                        XRCHECK(xrStringToPath(instance,
                                               subactionsVal[i].get<std::string>().c_str(),
                                               &subactions[i]));

                        if (actionInternal.isPoseAction)
                        {
                            XrActionSpaceCreateInfo asci{ XR_TYPE_ACTION_SPACE_CREATE_INFO };
                            asci.poseInActionSpace.orientation = XrQuaternionf{ 0.0f, 0.0f, 0.0f, 1.0f };
                            asci.subactionPath = subactions[i];

                            XrSpace poseSpace;
                            XRCHECK(xrCreateActionSpace(session, &asci, &poseSpace));
                            actionInternal.poseSubactionSpaces.insert({ subactions[i], poseSpace });
                        }
                    }
                }
                else if (actionInternal.isPoseAction)
                {
                    XrActionSpaceCreateInfo asci{ XR_TYPE_ACTION_SPACE_CREATE_INFO };
                    asci.poseInActionSpace.orientation = XrQuaternionf{ 0.0f, 0.0f, 0.0f, 1.0f };
                    XRCHECK(xrCreateActionSpace(session, &asci, &actionInternal.poseSpace));
                }

                XRCHECK(xrCreateAction(actionSet, &actionCreateInfo, &actionInternal.action));
                actionsInternal.insert({ (uint64_t)actionInternal.action, actionInternal });
                actions.insert({ actionName, (uint64_t)actionInternal.action });
            }
        }

        ~ActionSet()
        {
            for (auto& pair : actions)
            {
                ActionInternal& ai = actionsInternal[pair.second];
                XRCHECK(xrDestroyAction(ai.action));
                if (ai.isPoseAction && ai.poseSubactionSpaces.size() == 0)
                {
                    XRCHECK(xrDestroySpace(ai.poseSpace));
                }
                else
                {
                    for (auto& spacePair : ai.poseSubactionSpaces)
                    {
                        XRCHECK(xrDestroySpace(spacePair.second));
                    }
                }
            }

            XRCHECK(xrDestroyActionSet(actionSet));
        }
    };

    PFN_xrGetVulkanGraphicsRequirementsKHR xrGetVulkanGraphicsRequirementsKHR;
    PFN_xrGetVulkanGraphicsDeviceKHR xrGetVulkanGraphicsDeviceKHR;
    PFN_xrGetVulkanInstanceExtensionsKHR xrGetVulkanInstanceExtensionsKHR;
    PFN_xrGetVulkanDeviceExtensionsKHR xrGetVulkanDeviceExtensionsKHR;
    PFN_xrGetVisibilityMaskKHR xrGetVisibilityMaskKHR;

    robin_hood::unordered_flat_map<std::string, ActionSet*> actionSets;

    OpenXRInterface::OpenXRInterface(const EngineInterfaces& interfaces)
        : interfaces(interfaces)
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
        extensions.push_back(XR_KHR_VISIBILITY_MASK_EXTENSION_NAME);

        instanceCreateInfo.enabledExtensionCount = extensions.size();
        instanceCreateInfo.enabledExtensionNames = extensions.data();

        std::vector<const char*> layers;
        //layers.push_back("XR_APILAYER_LUNARG_core_validation");

        instanceCreateInfo.enabledApiLayerCount = layers.size();
        instanceCreateInfo.enabledApiLayerNames = layers.data();

        XRCHECK(xrCreateInstance(&instanceCreateInfo, &instance));

        // Set up the function pointers to things not included by the default OpenXR loader
#define LOAD_XR_FUNC(name) xrGetInstanceProcAddr(instance, #name, (PFN_xrVoidFunction*)&name);
        LOAD_XR_FUNC(xrGetVulkanGraphicsRequirementsKHR);
        LOAD_XR_FUNC(xrGetVulkanGraphicsDeviceKHR);
        LOAD_XR_FUNC(xrGetVulkanInstanceExtensionsKHR);
        LOAD_XR_FUNC(xrGetVulkanDeviceExtensionsKHR);
        LOAD_XR_FUNC(xrGetVisibilityMaskKHR);
#undef LOAD_XR_FUNC

        // Get the XR system
        XrSystemGetInfo systemGetInfo{ XR_TYPE_SYSTEM_GET_INFO };
        systemGetInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

        XrResult getSystemResult = xrGetSystem(instance, &systemGetInfo, &systemId);

        if (getSystemResult == XR_ERROR_FORM_FACTOR_UNAVAILABLE)
        {
            fatalErr("Couldn't find HMD!");
        }
        else
        {
            XRCHECK(getSystemResult);
        }

        XrSystemProperties systemProperties{ XR_TYPE_SYSTEM_PROPERTIES };
        XRCHECK(xrGetSystemProperties(instance, systemId, &systemProperties));

        logMsg("XR System name: %s (vendor ID %u)", systemProperties.systemName, systemProperties.vendorId);
    }

    std::vector<std::string> parseExtensionString(std::string extString)
    {
        std::vector<std::string> extList;

        auto it = extString.begin();
        while (it < extString.end())
        {
            auto extStart = it;

            while (*it != ' ')
            {
                it++;
                if (it == extString.end()) break;
            }

            std::string ext;
            ext.assign(extStart, it);
            extList.push_back(ext);
            it++;
        }

        return extList;
    }

    std::vector<std::string> OpenXRInterface::getRequiredInstanceExtensions()
    {
        uint32_t bufferSize;
        XRCHECK(xrGetVulkanInstanceExtensionsKHR(instance, systemId, 0, &bufferSize, nullptr));

        std::string result;
        result.resize(bufferSize, ' ');
        XRCHECK(xrGetVulkanInstanceExtensionsKHR(instance, systemId, bufferSize, &bufferSize, result.data()));

        return parseExtensionString(result);
    }

    std::vector<std::string> OpenXRInterface::getRequiredDeviceExtensions()
    {
        uint32_t bufferSize;
        XRCHECK(xrGetVulkanDeviceExtensionsKHR(instance, systemId, 0, &bufferSize, nullptr));

        std::string result;
        result.resize(bufferSize, ' ');
        XRCHECK(xrGetVulkanDeviceExtensionsKHR(instance, systemId, bufferSize, &bufferSize, result.data()));

        return parseExtensionString(result);
    }

    void OpenXRInterface::init()
    {
        VKRenderer* renderer = (VKRenderer*)interfaces.renderer;
        const R2::VK::Handles* handles = renderer->getCore()->GetHandles();

        XrGraphicsRequirementsVulkanKHR requirements{ XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR };
        XRCHECK(xrGetVulkanGraphicsRequirementsKHR(instance, systemId, &requirements));

        VkPhysicalDevice device;
        XRCHECK(xrGetVulkanGraphicsDeviceKHR(instance, systemId, handles->Instance, &device));

        if (device != handles->PhysicalDevice)
        {
            fatalErr("OpenXR's desired graphics device didn't match the graphics device R2 picked!");
        }

        // Set up the Vulkan graphics binding
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

        // Create the Stage and View reference spaces that we need.
        XrReferenceSpaceCreateInfo referenceSpaceCreateInfo { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
        referenceSpaceCreateInfo.poseInReferenceSpace.orientation = { 0.0f, 0.0f, 0.0f, 1.0f };

        XRCHECK(xrCreateReferenceSpace(session, &referenceSpaceCreateInfo, &stageReferenceSpace));

        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;

        XRCHECK(xrCreateReferenceSpace(session, &referenceSpaceCreateInfo, &viewReferenceSpace));

        XrViewConfigurationProperties viewConfigProps{ XR_TYPE_VIEW_CONFIGURATION_PROPERTIES };
        XRCHECK(xrGetViewConfigurationProperties(instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, &viewConfigProps));

        // Enumerate the view configurations
        uint32_t viewCount;
        XRCHECK(xrEnumerateViewConfigurationViews(
                instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                0, &viewCount, nullptr));

        if (viewCount == 0)
        {
            fatalErr("Nooo views?! <:(");
        }

        viewConfigViews.resize(viewCount, { XR_TYPE_VIEW_CONFIGURATION_VIEW });
        views.resize(viewCount, { XR_TYPE_VIEW });

        XRCHECK(xrEnumerateViewConfigurationViews(
                instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                (uint32_t)viewConfigViews.size(), &viewCount, viewConfigViews.data()));

        // Make sure the right swapchain format is supported
        std::vector<int64_t> swapchainFormats;
        uint32_t numSwapchainFormats;
        XRCHECK(xrEnumerateSwapchainFormats(session, 0, &numSwapchainFormats, nullptr));
        swapchainFormats.resize(numSwapchainFormats);
        XRCHECK(xrEnumerateSwapchainFormats(session, numSwapchainFormats, &numSwapchainFormats,
                                            swapchainFormats.data()));

        // For now, we're only going to support R8G8B8A8_SRGB. I think this'll be fine since this is
        // the format that's most likely to be supported, but in case this is required somewhere,
        // it shouldn't be too difficult to fix up.
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

        // Create the OpenXR swapchains
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

    void OpenXRInterface::updateEvents()
    {
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
                        sessionRunning = true;
                    }
                    else if (evt->state == XR_SESSION_STATE_STOPPING)
                    {
                        XRCHECK(xrEndSession(session));
                        sessionRunning = false;
                    }
                    else
                    {
                        logWarn("Unhandled OpenXR session state: %i", evt->state);
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

        if (actionSets.size() > 0)
        {
            ActionSet* activeSet = actionSets[activeActionSet];
            XrActiveActionSet activeActionSet{ activeSet->actionSet, (XrPath)XR_NULL_HANDLE };
            XrActionsSyncInfo actionSyncInfo{ XR_TYPE_ACTIONS_SYNC_INFO };
            actionSyncInfo.countActiveActionSets = 1;
            actionSyncInfo.activeActionSets = &activeActionSet;
            XRCHECK(xrSyncActions(session, &actionSyncInfo));
        }
    }

    void OpenXRInterface::getRenderResolution(uint32_t* x, uint32_t* y)
    {
        *x = viewConfigViews[0].recommendedImageRectWidth;
        *y = viewConfigViews[0].recommendedImageRectHeight;
    }

    const UnscaledTransform& OpenXRInterface::getEyeTransform(Eye eye)
    {
        switch (eye)
        {
            default:
            case Eye::LeftEye:
                return leftEyeTransform;
            case Eye::RightEye:
                return rightEyeTransform;
        }
    }

    const UnscaledTransform& OpenXRInterface::getHmdTransform()
    {
        return hmdTransform;
    }

    const glm::mat4& OpenXRInterface::getEyeProjectionMatrix(Eye eye)
    {
        switch (eye)
        {
            default:
            case Eye::LeftEye:
                return leftEyeProjectionMatrix;
            case Eye::RightEye:
                return rightEyeProjectionMatrix;
        }
    }

    bool OpenXRInterface::getHiddenAreaMesh(Eye eye, HiddenAreaMesh& mesh)
    {
        XrVisibilityMaskKHR mask { XR_TYPE_VISIBILITY_MASK_KHR };
        XRCHECK(xrGetVisibilityMaskKHR(session,
                                       XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, (int)eye,
                                       XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR, &mask));

        if (mask.vertexCountOutput == 0 || mask.indexCountOutput == 0)
        {
            return false;
        }

        mesh.indices.resize(mask.indexCountOutput);
        mesh.verts.resize(mask.vertexCountOutput);
        mask.vertexCapacityInput = mesh.verts.size();
        mask.indexCapacityInput = mesh.indices.size();
        mask.indices = mesh.indices.data();
        mask.vertices = (XrVector2f*)mesh.verts.data();

        XRCHECK(xrGetVisibilityMaskKHR(session,
                                       XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, (int)eye,
                                       XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR, &mask));

        return true;
    }

    void OpenXRInterface::loadActionJson(const char* path)
    {
        auto loadResult = LoadFileToString(path);
        if (loadResult.error != IOError::None)
        {
            logWarn("Failed to load action json %s", path);
            return;
        }

        XrActionSet activeSet = XR_NULL_HANDLE;
        nlohmann::json j = nlohmann::json::parse(loadResult.value);
        for (auto& actionSetPair : j["action_sets"].items())
        {
            ActionSet* set = new ActionSet(instance, session, actionSetPair.key(), actionSetPair.value());
            actionSets.insert({ actionSetPair.key(), set });
            activeActionSet = actionSetPair.key();
            activeSet = set->actionSet;
        }

        if (activeSet == XR_NULL_HANDLE)
        {
            logErr("Action json file didn't have an action set!");
            return;
        }

        for (auto& suggestedBindingPair : j["suggested_bindings"].items())
        {
            std::string interactionProfile = suggestedBindingPair.key();
            XrInteractionProfileSuggestedBinding suggestedBinding
                { XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };

            suggestedBinding.interactionProfile = getXrPath(interactionProfile.c_str());

            std::vector<XrActionSuggestedBinding> suggestedActionBindings;

            for (auto& actionBinding : suggestedBindingPair.value().items())
            {
                std::string actionPath = actionBinding.key();
                size_t secondSlashPos = actionPath.find('/', 1);
                std::string actionSetName = actionPath.substr(1, secondSlashPos - 1);
                std::string actionName = actionPath.substr(secondSlashPos);

                ActionSet* set = actionSets[actionSetName];
                XrAction action = (XrAction)set->actions[actionName];

                for (auto& inputSource : actionBinding.value())
                {
                    XrActionSuggestedBinding asb{ action };
                    asb.binding = getXrPath(inputSource.get<std::string>().c_str());
                    suggestedActionBindings.push_back(asb);
                }
            }

            suggestedBinding.countSuggestedBindings = suggestedActionBindings.size();
            suggestedBinding.suggestedBindings = suggestedActionBindings.data();

            XRCHECK(xrSuggestInteractionProfileBindings(instance, &suggestedBinding));
        }

        XrSessionActionSetsAttachInfo attachInfo{ XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
        attachInfo.actionSets = &activeSet;
        attachInfo.countActionSets = 1;
        XRCHECK(xrAttachSessionActionSets(session, &attachInfo));
    }

    uint64_t OpenXRInterface::getActionHandle(const char* actionSet, const char* action)
    {
        auto setIterator = actionSets.find(actionSet);

        if (setIterator == actionSets.end())
        {
            logErr("Couldn't find action set %s", actionSet);
            return UINT64_MAX;
        }

        auto actionIterator = setIterator->second->actions.find(action);

        if (actionIterator == setIterator->second->actions.end())
        {
            logErr("Couldn't find action %s", action);
            return UINT64_MAX;
        }

        return actionIterator->second;
    }

    uint64_t OpenXRInterface::getSubactionHandle(const char* subaction)
    {
        return (uint64_t)getXrPath(subaction);
    }

    BooleanActionState OpenXRInterface::getBooleanActionState(uint64_t actionHandle, uint64_t subactionHandle)
    {
        auto action = (XrAction)actionHandle;
        XrActionStateGetInfo stateGetInfo { XR_TYPE_ACTION_STATE_GET_INFO };
        stateGetInfo.action = action;
        if (subactionHandle != UINT64_MAX)
        {
            stateGetInfo.subactionPath = (XrPath)subactionHandle;
        }

        XrActionStateBoolean xrState{ XR_TYPE_ACTION_STATE_BOOLEAN };
        XRCHECK(xrGetActionStateBoolean(session, &stateGetInfo, &xrState));

        BooleanActionState state{};
        state.changedSinceLastFrame = xrState.changedSinceLastSync;
        state.currentState = xrState.currentState;

        return state;
    }

    FloatActionState OpenXRInterface::getFloatActionState(uint64_t actionHandle, uint64_t subactionHandle)
    {
        auto action = (XrAction)actionHandle;
        XrActionStateGetInfo stateGetInfo { XR_TYPE_ACTION_STATE_GET_INFO };
        stateGetInfo.action = action;
        if (subactionHandle != UINT64_MAX)
        {
            stateGetInfo.subactionPath = (XrPath)subactionHandle;
        }

        XrActionStateFloat xrState{ XR_TYPE_ACTION_STATE_FLOAT };
        XRCHECK(xrGetActionStateFloat(session, &stateGetInfo, &xrState));

        FloatActionState state{};
        state.changedSinceLastFrame = xrState.changedSinceLastSync;
        state.currentState = xrState.currentState;

        return state;
    }

    Vector2fActionState OpenXRInterface::getVector2fActionState(uint64_t actionHandle, uint64_t subactionHandle)
    {
        auto action = (XrAction)actionHandle;
        XrActionStateGetInfo stateGetInfo { XR_TYPE_ACTION_STATE_GET_INFO };
        stateGetInfo.action = action;
        if (subactionHandle != UINT64_MAX)
        {
            stateGetInfo.subactionPath = (XrPath)subactionHandle;
        }

        XrActionStateVector2f xrState{ XR_TYPE_ACTION_STATE_VECTOR2F };
        XRCHECK(xrGetActionStateVector2f(session, &stateGetInfo, &xrState));

        Vector2fActionState state{};
        state.changedSinceLastFrame = xrState.changedSinceLastSync;
        state.currentState = glm::vec2(xrState.currentState.x, xrState.currentState.y);

        return state;
    }

    UnscaledTransform OpenXRInterface::getPoseActionState(uint64_t actionHandle, uint64_t subactionHandle)
    {
        const ActionInternal& actionInternal = actionsInternal[actionHandle];

        XrSpaceLocation spaceLocation{ XR_TYPE_SPACE_LOCATION };
        XrSpace poseSpace;

        if (subactionHandle != UINT64_MAX)
        {
            poseSpace = actionInternal.poseSpace;
        }
        else
        {
            poseSpace = actionInternal.poseSubactionSpaces.at((XrPath)subactionHandle);
        }

        XRCHECK(xrLocateSpace(poseSpace, stageReferenceSpace, nextDisplayTime, &spaceLocation));

        UnscaledTransform transform{};
        transform.position = glm::make_vec3(&spaceLocation.pose.position.x);

        XrQuaternionf rot = spaceLocation.pose.orientation;
        transform.rotation = glm::quat{rot.w, rot.x, rot.y, rot.z};

        return transform;
    }

    void OpenXRInterface::beginFrame()
    {
        if (!sessionRunning) return;
        XrFrameBeginInfo frameBeginInfo{ XR_TYPE_FRAME_BEGIN_INFO };
        XRCHECK(xrBeginFrame(session, &frameBeginInfo));
    }

    void composeProjection(XrFovf fov, float zNear, glm::mat4& p)
    {
        float left = tanf(fov.angleLeft);
        float right = tanf(fov.angleRight);
        float up = tanf(fov.angleUp);
        float down = tanf(fov.angleDown);

        float idx = 1.0f / (right - left);
        float idy = 1.0f / (up - down);
        float sx = right + left;
        float sy = down + up;

        p[0][0] = 2 * idx; p[1][0] = 0;       p[2][0] = sx * idx;    p[3][0] = 0;
        p[0][1] = 0;       p[1][1] = 2 * idy; p[2][1] = sy * idy;    p[3][1] = 0;
        p[0][2] = 0;       p[1][2] = 0;       p[2][2] = 0.0f; p[3][2] = zNear;
        p[0][3] = 0;       p[1][3] = 0;       p[2][3] = -1.0f;       p[3][3] = 0;
    }

    void OpenXRInterface::waitFrame()
    {
        if (!sessionRunning) return;
        XrFrameWaitInfo waitInfo{ XR_TYPE_FRAME_WAIT_INFO };
        XrFrameState frameState{ XR_TYPE_FRAME_STATE };
        XRCHECK(xrWaitFrame(session, &waitInfo, &frameState));
        nextDisplayTime = frameState.predictedDisplayTime;

        // Get the pose of each eye
        XrViewLocateInfo viewLocateInfo{ XR_TYPE_VIEW_LOCATE_INFO };
        viewLocateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        viewLocateInfo.displayTime = nextDisplayTime;
        viewLocateInfo.space = stageReferenceSpace;

        XrViewState viewState{ XR_TYPE_VIEW_STATE };
        uint32_t numViews;
        XRCHECK(xrLocateViews(session, &viewLocateInfo, &viewState, views.size(), &numViews, views.data()));

        if (enumHasFlag(viewState.viewStateFlags, XR_VIEW_STATE_ORIENTATION_VALID_BIT))
        {
            XrQuaternionf lO = views[0].pose.orientation;
            leftEyeTransform.rotation = glm::quat{lO.w, lO.x, lO.y, lO.z};

            XrQuaternionf rO = views[1].pose.orientation;
            rightEyeTransform.rotation = glm::quat{rO.w, rO.x, rO.y, rO.z};
        }

        if (enumHasFlag(viewState.viewStateFlags, XR_VIEW_STATE_POSITION_VALID_BIT))
        {
            leftEyeTransform.position = glm::make_vec3(&views[0].pose.position.x);
            rightEyeTransform.position = glm::make_vec3(&views[1].pose.position.x);
        }

        // Get the pose of the HMD
        XrSpaceLocation spaceLocation{ XR_TYPE_SPACE_LOCATION };
        XRCHECK(xrLocateSpace(viewReferenceSpace, stageReferenceSpace, nextDisplayTime, &spaceLocation));
        if (enumHasFlag(spaceLocation.locationFlags, XR_SPACE_LOCATION_ORIENTATION_VALID_BIT))
        {
            XrQuaternionf o = spaceLocation.pose.orientation;
            hmdTransform.rotation = glm::quat{o.w, o.x, o.y, o.z};
        }

        if (enumHasFlag(spaceLocation.locationFlags, XR_SPACE_LOCATION_POSITION_VALID_BIT))
        {
            hmdTransform.position = glm::make_vec3(&spaceLocation.pose.position.x);
        }

        leftEyeProjectionMatrix = glm::mat4{ 1.0f };
        rightEyeProjectionMatrix = glm::mat4{ 1.0f };
        composeProjection(views[0].fov, 0.01f, leftEyeProjectionMatrix);
        composeProjection(views[1].fov, 0.01f, rightEyeProjectionMatrix);
    }

    void OpenXRInterface::submitLayered(R2::VK::CommandBuffer& cb, R2::VK::Texture* texture)
    {
        if (!sessionRunning) return;
        texture->Acquire(cb,
                         VK::ImageLayout::TransferSrcOptimal,
                         VK::AccessFlags::TransferRead, VK::PipelineStageFlags::Transfer);

        for (int i = 0; i < 2; i++)
        {
            XrSwapchainImageAcquireInfo acquireInfo{ XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };

            uint32_t imageIndex;
            XRCHECK(xrAcquireSwapchainImage(swapchains[i].swapchain, &acquireInfo, &imageIndex));

            XrSwapchainImageWaitInfo waitInfo{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
            waitInfo.timeout = XR_INFINITE_DURATION;
            XRCHECK(xrWaitSwapchainImage(swapchains[i].swapchain, &waitInfo));

            VkImageCopy imageCopy{};
            imageCopy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageCopy.srcSubresource.baseArrayLayer = i;
            imageCopy.srcSubresource.layerCount = 1;
            imageCopy.srcSubresource.mipLevel = 0;

            imageCopy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageCopy.dstSubresource.baseArrayLayer = 0;
            imageCopy.dstSubresource.layerCount = 1;
            imageCopy.dstSubresource.mipLevel = 0;

            imageCopy.extent.width = texture->GetWidth();
            imageCopy.extent.height = texture->GetHeight();
            imageCopy.extent.depth = 1;

            VkImage destImage = swapchains[i].images[imageIndex].image;

            VkImageMemoryBarrier imb{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            imb.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            imb.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imb.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            imb.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imb.image = destImage;
            imb.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

            vkCmdPipelineBarrier(cb.GetNativeHandle(),
                                 VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0, 0, nullptr, 0, nullptr, 1, &imb);

            vkCmdCopyImage(cb.GetNativeHandle(),
               texture->GetNativeHandle(),
               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
               destImage,
               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
               1, &imageCopy);

            imb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imb.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
            imb.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imb.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            vkCmdPipelineBarrier(cb.GetNativeHandle(),
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                                 0, 0, nullptr, 0, nullptr, 1, &imb);

            XrSwapchainImageReleaseInfo releaseInfo{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
            XRCHECK(xrReleaseSwapchainImage(swapchains[i].swapchain, &releaseInfo));
        }
    }

    void OpenXRInterface::endFrame()
    {
        if (!sessionRunning) return;
        XrCompositionLayerProjectionView layerViews[2] { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW };

        for (int i = 0; i < 2; i++)
        {
            layerViews[i].fov = views[i].fov;
            layerViews[i].pose = views[i].pose;
            layerViews[i].subImage.swapchain = swapchains[i].swapchain;
            layerViews[i].subImage.imageArrayIndex = 0;
            layerViews[i].subImage.imageRect.extent = {
                    (int)viewConfigViews[i].recommendedImageRectWidth,
                    (int)viewConfigViews[i].recommendedImageRectHeight
            };
        }

        XrCompositionLayerProjection layer{ XR_TYPE_COMPOSITION_LAYER_PROJECTION };
        layer.space = stageReferenceSpace;
        layer.viewCount = 2;
        layer.views = layerViews;

        XrCompositionLayerProjection* layerPtr = &layer;

        XrFrameEndInfo frameEndInfo{ XR_TYPE_FRAME_END_INFO };
        frameEndInfo.displayTime = nextDisplayTime;
        frameEndInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        frameEndInfo.layers = (XrCompositionLayerBaseHeader**)&layerPtr;
        frameEndInfo.layerCount = 1;
        XRCHECK(xrEndFrame(session, &frameEndInfo));
    }

    XrPath OpenXRInterface::getXrPath(const char* path)
    {
        XrPath ret;
        XRCHECK(xrStringToPath(instance, path, &ret));
        return ret;
    }
}