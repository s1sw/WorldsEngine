#pragma once
#include <openvr.h>
#include <vector>
#include <string>
#include <sstream>
#include <vulkan/vulkan.h>
#include <SDL2/SDL_messagebox.h>

class OpenVRInterface : public IVRInterface {
    vr::IVRSystem* system;
public:
    void init() {
        vr::EVRInitError eError = vr::VRInitError_None;
        system = vr::VR_Init(&eError, vr::VRApplication_Scene);

        if (eError != vr::VRInitError_None) {
            char buf[1024];
            sprintf_s(buf, sizeof(buf), "Unable to init VR runtime: %s", vr::VR_GetVRInitErrorAsEnglishDescription(eError));
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "VR_Init Failed", buf, NULL);
        }
    }

    std::vector<std::string> getVulkanInstanceExtensions() {
        uint32_t extCharCount = vr::VRCompositor()->GetVulkanInstanceExtensionsRequired(nullptr, 0);

        std::string str((size_t)extCharCount, 'Z');

        vr::VRCompositor()->GetVulkanInstanceExtensionsRequired(str.data(), (uint32_t)str.size());

        std::vector<std::string> extensions;

        std::istringstream stringStream(str.substr(0, extCharCount));
        std::string currExt;
        while (std::getline(stringStream, currExt, ' ')) {
            extensions.push_back(currExt);
        }

        return extensions;
    }

    std::vector<std::string> getVulkanDeviceExtensions(VkPhysicalDevice physDevice) {
        uint32_t extCharCount = vr::VRCompositor()->GetVulkanDeviceExtensionsRequired(physDevice, nullptr, 0);

        std::string str((size_t)extCharCount, 'Z');

        vr::VRCompositor()->GetVulkanDeviceExtensionsRequired(physDevice, str.data(), (uint32_t)str.size());

        std::vector<std::string> extensions;

        std::istringstream stringStream(str);
        std::string currExt;
        while (std::getline(stringStream, currExt, ' ')) {
            extensions.push_back(currExt);
        }

        return extensions;
    }

    void getRenderResolution(uint32_t* x, uint32_t* y) {
        system->GetRecommendedRenderTargetSize(x, y);
    }

    glm::mat4 toMat4(vr::HmdMatrix34_t mat) {
        return glm::mat4(
            mat.m[0][0], mat.m[1][0], mat.m[2][0], 0.0f,
            mat.m[0][1], mat.m[1][1], mat.m[2][1], 0.0f,
            mat.m[0][2], mat.m[1][2], mat.m[2][2], 0.0f,
            mat.m[0][3], mat.m[1][3], mat.m[2][3], 1.0f);
    }

    glm::mat4 toMat4(vr::HmdMatrix44_t mat) {
        return glm::mat4(
            mat.m[0][0], mat.m[1][0], mat.m[2][0], mat.m[3][0],
            mat.m[0][1], mat.m[1][1], mat.m[2][1], mat.m[3][1],
            mat.m[0][2], mat.m[1][2], mat.m[2][2], mat.m[3][2],
            mat.m[0][3], mat.m[1][3], mat.m[2][3], mat.m[3][3]);
    }

    glm::mat4 getViewMat(vr::EVREye eye) {
        return toMat4(system->GetEyeToHeadTransform(eye));
    }

    glm::mat4 getProjMat(vr::EVREye eye, float near, float far) {
        return toMat4(system->GetProjectionMatrix(eye, near, far));
    }
};