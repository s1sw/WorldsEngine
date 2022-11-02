#pragma once
#include <stdint.h>
#include <vector>
#include <string>
#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>
#include <Core/Transform.hpp>

#define XR_DEFINE_HANDLE(object) typedef struct object##_T* object;
XR_DEFINE_HANDLE(XrInstance)
XR_DEFINE_HANDLE(XrSession)
XR_DEFINE_HANDLE(XrSpace)
XR_DEFINE_HANDLE(XrSwapchain)
// The XR_DEFINE_HANDLE and VK_DEFINE_HANDLE macros are identical
XR_DEFINE_HANDLE(VkPhysicalDevice)
#undef XR_DEFINE_HANDLE
#define XR_DEFINE_ATOM(object) typedef uint64_t object;
XR_DEFINE_ATOM(XrSystemId)
XR_DEFINE_ATOM(XrPath)
#undef XR_DEFINE_ATOM

struct XrView;
struct XrViewConfigurationView;

namespace R2::VK
{
    class CommandBuffer;
    class Texture;
}

namespace worlds
{
    struct EngineInterfaces;

    enum class TrackedObject
    {
        HMD,
        LeftHand,
        RightHand
    };

    enum class Hand
    {
        LeftHand,
        RightHand
    };

    enum class Eye
    {
        LeftEye,
        RightEye
    };

    struct HiddenAreaMesh
    {
        std::vector<glm::vec2> verts;
        std::vector<uint32_t> indices;
    };

    struct BooleanActionState
    {
        bool currentState;
        bool changedSinceLastFrame;
    };

    struct FloatActionState
    {
        float currentState;
        bool changedSinceLastFrame;
    };

    struct Vector2fActionState
    {
        glm::vec2 currentState;
        bool changedSinceLastFrame;
    };

    class ActionSet;

    class OpenXRInterface
    {
    public:
        OpenXRInterface(const EngineInterfaces& interfaces);
        ~OpenXRInterface();

        std::vector<std::string> getRequiredInstanceExtensions();
        std::vector<std::string> getRequiredDeviceExtensions();
        void init();

        void getRenderResolution(uint32_t* x, uint32_t* y);
        const UnscaledTransform& getEyeTransform(Eye eye);
        const UnscaledTransform& getHmdTransform();
        const glm::mat4& getEyeProjectionMatrix(Eye eye);
        bool getHiddenAreaMesh(Eye eye, HiddenAreaMesh& mesh);

        // Input
        void loadActionJson(const char* path);
        uint64_t getActionHandle(const char* actionSet, const char* action);
        uint64_t getSubactionHandle(const char* subaction);
        BooleanActionState getBooleanActionState(uint64_t actionHandle, uint64_t subactionHandle = 0);
        FloatActionState getFloatActionState(uint64_t actionHandle, uint64_t subactionHandle = 0);
        Vector2fActionState getVector2fActionState(uint64_t actionHandle, uint64_t subactionHandle = 0);
        UnscaledTransform getPoseActionState(uint64_t actionHandle, uint64_t subactionHandle = 0);
        void applyHapticFeedback(float duration, float frequency, float amplitude, uint64_t actionHandle, uint64_t subactionHandle = 0);

        void waitFrame();
        void beginFrame();
        void submitLayered(R2::VK::CommandBuffer& cb, R2::VK::Texture* texture);
        void endFrame();
        void updateEvents();
    private:
        class OpenXRSwapchain;
        const EngineInterfaces& interfaces;
        XrInstance instance;
        XrSystemId systemId;
        XrSession session;
        XrSpace stageReferenceSpace;
        XrSpace viewReferenceSpace;
        std::vector<OpenXRSwapchain> swapchains;
        std::vector<XrView> views;
        std::vector<XrViewConfigurationView> viewConfigViews;
        int64_t nextDisplayTime;
        UnscaledTransform leftEyeTransform;
        UnscaledTransform rightEyeTransform;
        UnscaledTransform hmdTransform;
        glm::mat4 leftEyeProjectionMatrix;
        glm::mat4 rightEyeProjectionMatrix;
        bool sessionRunning = false;

        // Input Stuff
        std::string activeActionSet;
        XrPath getXrPath(const char* path);
    };
}