#pragma once
#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr.h>
#define XR_NO_PROTOTYPES
#include <openxr/openxr_platform.h>
#include <SDL2/SDL_log.h>
#include <cstring>
#include <vector>
#include <sstream>
#include <iostream>
#include "IVRInterface.hpp"

namespace worlds {
    // TODO: Finish OpenXR when SteamVR's implementation of it
    // actually works
    class XRInterface : public IVRInterface {
        XrInstance instance;
        XrSystemId sysId;
        XrSession session;
        XrViewConfigurationProperties viewConfigProps;
        std::vector<XrViewConfigurationView> viewConfigViews;
        PFN_xrGetVulkanInstanceExtensionsKHR xrGetVulkanInstanceExtensionsKHR;
        PFN_xrGetVulkanDeviceExtensionsKHR xrGetVulkanDeviceExtensionsKHR;

        static XrBool32 debugCallback(XrDebugUtilsMessageSeverityFlagsEXT messageSeverity,
            XrDebugUtilsMessageTypeFlagsEXT messageTypes,
            const XrDebugUtilsMessengerCallbackDataEXT* callbackData,
            void* userData) {
            std::cout << "OpenXR Debug: " << callbackData->message << "\n";
            return XR_TRUE;
        }

    public:
        void checkResult(XrResult res, const char* errMsg) {
            if (!XR_SUCCEEDED(res))
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, errMsg, res);
        }

        void checkResult(XrResult res) {
            checkResult(res, "OpenXR error: %d");
        }

        void initXR() {
            uint32_t numExtensionProps = 0;
            checkResult(xrEnumerateInstanceExtensionProperties(nullptr, 0, &numExtensionProps, nullptr));

            std::vector<XrExtensionProperties> extensionProps(numExtensionProps);

            // We have to set the type for the extension property to be considered valid!
            // This isn't set automatically by EnumerateInstanceExtensionProperties and not setting it causes an error.
            for (uint32_t i = 0; i < numExtensionProps; i++) {
                extensionProps[i].type = XR_TYPE_EXTENSION_PROPERTIES;
            }

            checkResult(xrEnumerateInstanceExtensionProperties(nullptr, (uint32_t)extensionProps.size(), &numExtensionProps, extensionProps.data()));

            bool foundVKExtension = false;

            for (auto& extProp : extensionProps) {
                SDL_Log("XR Extension %s, version %d", extProp.extensionName, extProp.extensionVersion);
                if (strcmp(extProp.extensionName, XR_KHR_VULKAN_ENABLE_EXTENSION_NAME) != 0) {
                    foundVKExtension = true;
                }
            }

            if (!foundVKExtension) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to find OpenXR Vulkan extension.");
                return;
            }

            XrInstanceCreateInfo ici{ XR_TYPE_INSTANCE_CREATE_INFO };
            XrApplicationInfo appInfo;

            appInfo.apiVersion = XR_CURRENT_API_VERSION;
            std::strcpy(appInfo.applicationName, "ExpEng");
            std::strcpy(appInfo.engineName, "ExpEng");
            appInfo.engineVersion = (uint32_t)(XR_MAKE_VERSION(1, 0, 0));
            appInfo.applicationVersion = (uint32_t)(XR_MAKE_VERSION(1, 0, 0));
            ici.applicationInfo = appInfo;
            ici.createFlags = 0;
            ici.next = nullptr;

            std::vector<const char*> extensions;

            extensions.push_back(XR_KHR_VULKAN_ENABLE_EXTENSION_NAME);

#ifndef NDEBUG
            extensions.push_back(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);

            XrDebugUtilsMessengerCreateInfoEXT mci;
            mci.type = XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            mci.messageSeverities = 0
                | XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
                | XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                | XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
                | XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
            mci.messageTypes = 0
                | XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                | XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT
                | XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
                | XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
            mci.userCallback = &XRInterface::debugCallback;
            mci.userData = nullptr;
            ici.next = &mci;
#endif
            ici.enabledExtensionCount = (uint32_t)extensions.size();
            ici.enabledExtensionNames = extensions.data();

            XrResult instanceCreateResult = xrCreateInstance(&ici, &instance);

            checkResult(instanceCreateResult, "Failed to create instance: %d");

            XrSystemGetInfo sgi{};
            sgi.type = XR_TYPE_SYSTEM_GET_INFO;
            sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
            sgi.next = nullptr;

            checkResult(xrGetSystem(instance, &sgi, &sysId), "Failed to get system ID: %d");
            XrSystemProperties sysProps;
            checkResult(xrGetSystemProperties(instance, sysId, &sysProps), "Failed to get system properties: %d");
            SDL_Log("XR system name: %s", sysProps.systemName);

            checkResult(xrGetInstanceProcAddr(instance, "xrGetVulkanInstanceExtensionsKHR", (PFN_xrVoidFunction*)&xrGetVulkanInstanceExtensionsKHR));
            checkResult(xrGetInstanceProcAddr(instance, "xrGetVulkanDeviceExtensionsKHR", (PFN_xrVoidFunction*)&xrGetVulkanDeviceExtensionsKHR));

            checkResult(xrGetViewConfigurationProperties(instance,
                sysId,
                XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                &viewConfigProps));

            uint32_t viewConfigViewCount;
            xrEnumerateViewConfigurationViews(instance, sysId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewConfigViewCount, nullptr);

            viewConfigViews.resize(viewConfigViewCount);

            xrEnumerateViewConfigurationViews(instance, sysId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, (uint32_t)viewConfigViews.size(), &viewConfigViewCount, viewConfigViews.data());
        }

        std::vector<std::string> getVulkanInstanceExtensions() {
            uint32_t extCharCount = 0;
            checkResult(xrGetVulkanInstanceExtensionsKHR(instance, sysId, 0, &extCharCount, nullptr), "Failed to get required"
                " Vulkan instance extension count: %d");

            std::string str((size_t)extCharCount, 'Z');

            checkResult(xrGetVulkanInstanceExtensionsKHR(instance, sysId, (uint32_t)str.size(), &extCharCount, str.data()));

            std::vector<std::string> extensions;

            std::istringstream stringStream(str.substr(0, extCharCount));
            std::string currExt;
            while (std::getline(stringStream, currExt, ' ')) {
                extensions.push_back(currExt);
            }

            // Filter the extensions because SteamVR's OpenXR implemntation is... buggy
            auto res = std::remove_if(extensions.begin(), extensions.end(),
                [](auto const extStr) { return extStr.find("VK_NV") != std::string::npos; });

            extensions.erase(res, extensions.end());


            return extensions;
        }

        std::vector<std::string> getVulkanDeviceExtensions() {
            uint32_t extCharCount;
            checkResult(xrGetVulkanDeviceExtensionsKHR(instance, sysId, 0, &extCharCount, nullptr), "Failed to get required"
                " Vulkan device extension count: %d");

            std::string str((size_t)extCharCount, 'Z');

            checkResult(xrGetVulkanDeviceExtensionsKHR(instance, sysId, (uint32_t)str.size(), &extCharCount, str.data()));

            std::vector<std::string> extensions;

            std::istringstream stringStream(str);
            std::string currExt;
            while (std::getline(stringStream, currExt, ' ')) {
                extensions.push_back(currExt);
            }

            // Filter the extensions because SteamVR's OpenXR implemntation is... buggy
            auto res = std::remove_if(extensions.begin(), extensions.end(),
                [](auto const extStr) { return extStr.find("VK_NV") != std::string::npos; });

            extensions.erase(res, extensions.end());

            return extensions;
        }

        void createSession(XrGraphicsBindingVulkanKHR graphicsBinding) {
            session = XR_NULL_HANDLE;
            XrSessionCreateInfo sci{ XR_TYPE_SESSION_CREATE_INFO };
            sci.systemId = sysId;
            sci.next = &graphicsBinding;

            checkResult(xrCreateSession(instance, &sci, &session), "Failed to create session %d");
        }

        void updateInput() override {

        }

        glm::vec2 getLocomotionInput() override {
            return glm::vec2{};
        }

        glm::mat4 getHeadTransform() override {
            return glm::mat4{};
        }

        bool getSprintInput() override {
            return false;
        }

        bool getJumpInput() override {
            return false;
        }
    };
}