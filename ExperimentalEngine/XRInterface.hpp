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

class XRInterface {
    XrInstance instance;
    XrSystemId sysId;
    PFN_xrGetVulkanInstanceExtensionsKHR xrGetVulkanInstanceExtensionsKHR;
    PFN_xrGetVulkanDeviceExtensionsKHR xrGetVulkanDeviceExtensionsKHR;

    static XrBool32 debugCallback(XrDebugUtilsMessageSeverityFlagsEXT messageSeverity,
        XrDebugUtilsMessageTypeFlagsEXT messageTypes,
        const XrDebugUtilsMessengerCallbackDataEXT* callbackData,
        void* userData) {

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
        for (int i = 0; i < numExtensionProps; i++) {
            extensionProps[i].type = XR_TYPE_EXTENSION_PROPERTIES;
        }

        checkResult(xrEnumerateInstanceExtensionProperties(nullptr, extensionProps.size(), &numExtensionProps, extensionProps.data()));

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
        appInfo.engineVersion = XR_MAKE_VERSION(1, 0, 0);
        appInfo.applicationVersion = XR_MAKE_VERSION(1, 0, 0);
        ici.applicationInfo = appInfo;
        ici.createFlags = 0;
        ici.next = nullptr;
        ici.enabledExtensionCount = 1;
        const char* extensions[] = { XR_KHR_VULKAN_ENABLE_EXTENSION_NAME };
        ici.enabledExtensionNames = extensions;

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
    }

    std::vector<std::string> getVulkanInstanceExtensions() {
        uint32_t extCharCount = 0;
        //checkResult(xrGetVulkanInstanceExtensionsKHR(instance, sysId, 0, &extCharCount, nullptr), "Failed to get required"
        //" Vulkan instance extension count: %d");

        std::string str((size_t)512, 'Z');
        
        checkResult(xrGetVulkanInstanceExtensionsKHR(instance, sysId, str.size(), &extCharCount, str.data()));

        std::vector<std::string> extensions;

        std::istringstream stringStream(str.substr(0, extCharCount));
        std::string currExt;
        while (std::getline(stringStream, currExt, ' ')) {
            extensions.push_back(currExt);
        }

        return extensions;
    }

    std::vector<std::string> getVulkanDeviceExtensions() {
        uint32_t extCharCount;
        checkResult(xrGetVulkanDeviceExtensionsKHR(instance, sysId, 0, &extCharCount, nullptr), "Failed to get required"
            " Vulkan device extension count: %d");

        std::string str((size_t)extCharCount, 'Z');

        checkResult(xrGetVulkanDeviceExtensionsKHR(instance, sysId, str.size(), &extCharCount, str.data()));

        std::vector<std::string> extensions;

        std::istringstream stringStream(str);
        std::string currExt;
        while (std::getline(stringStream, currExt, ' ')) {
            extensions.push_back(currExt);
        }

        return extensions;
    }
};