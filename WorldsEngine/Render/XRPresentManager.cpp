#include <Render/RenderInternal.hpp>
#include <R2/VK.hpp>
#include <openvr.h>

using namespace R2;

namespace worlds
{
    void XRPresentManager::createTextures()
    {
        VK::TextureCreateInfo tci = VK::TextureCreateInfo::RenderTarget2D(VK::TextureFormat::R8G8B8A8_SRGB, width, height);
        leftEye = core->CreateTexture(tci);
        rightEye = core->CreateTexture(tci);
    }

    XRPresentManager::XRPresentManager(VK::Core* core, int width, int height)
        : width(width)
        , height(height)
        , core(core)
    {
        if (width == 0 || height == 0)
        {
            uint32_t uWidth, uHeight;
            vr::VRSystem()->GetRecommendedRenderTargetSize(&uWidth, &uHeight);
            this->width = (int)uWidth;
            this->height = (int)uHeight;
        }

        createTextures();
    }

    void XRPresentManager::resize(int width, int height)
    {
        this->width = width;
        this->height = height;
        createTextures();
    }

    void XRPresentManager::copyFromLayered(VK::CommandBuffer cb, VK::Texture* layeredTexture)
    {
        VK::TextureBlit leftBlit{};
        leftBlit.Source.LayerCount = 1;
        leftBlit.Destination.LayerCount = 1;

        leftBlit.SourceOffsets[1].X = layeredTexture->GetWidth();
        leftBlit.SourceOffsets[1].Y = layeredTexture->GetHeight();
        leftBlit.SourceOffsets[1].Z = 1;

        leftBlit.DestinationOffsets[1].X = leftEye->GetWidth();
        leftBlit.DestinationOffsets[1].Y = leftEye->GetHeight();
        leftBlit.DestinationOffsets[1].Z = 1;

        cb.TextureBlit(layeredTexture, leftEye.Get(), leftBlit);

        VK::TextureBlit rightBlit{};
        rightBlit.Source.LayerCount = 1;
        rightBlit.Source.LayerStart = 1;
        rightBlit.Destination.LayerCount = 1;

        rightBlit.SourceOffsets[1].X = layeredTexture->GetWidth();
        rightBlit.SourceOffsets[1].Y = layeredTexture->GetHeight();
        rightBlit.SourceOffsets[1].Z = 1;

        rightBlit.DestinationOffsets[1].X = rightEye->GetWidth();
        rightBlit.DestinationOffsets[1].Y = rightEye->GetHeight();
        rightBlit.DestinationOffsets[1].Z = 1;

        cb.TextureBlit(layeredTexture, rightEye.Get(), rightBlit);
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


    void XRPresentManager::submit(glm::mat4 usedPose)
    {
        const VK::Handles* handles = core->GetHandles();

        VkImage leftNativeHandle = leftEye->GetNativeHandle();
        VkImage rightNativeHandle = rightEye->GetNativeHandle();

        vr::VRVulkanTextureData_t leftVKTexData{};
        leftVKTexData.m_nImage = (uint64_t)leftNativeHandle;
        leftVKTexData.m_pDevice = handles->Device;
        leftVKTexData.m_pPhysicalDevice = handles->PhysicalDevice;
        leftVKTexData.m_pInstance = handles->Instance;
        leftVKTexData.m_pQueue = handles->Queues.Graphics;
        leftVKTexData.m_nQueueFamilyIndex = handles->Queues.GraphicsFamilyIndex;
        leftVKTexData.m_nWidth = leftEye->GetWidth();
        leftVKTexData.m_nHeight = leftEye->GetHeight();
        leftVKTexData.m_nFormat = (VkFormat)leftEye->GetFormat();
        leftVKTexData.m_nSampleCount = 1;

        vr::VRVulkanTextureData_t rightVKTexData{};
        rightVKTexData.m_nImage = (uint64_t)rightNativeHandle;
        rightVKTexData.m_pDevice = handles->Device;
        rightVKTexData.m_pPhysicalDevice = handles->PhysicalDevice;
        rightVKTexData.m_pInstance = handles->Instance;
        rightVKTexData.m_pQueue = handles->Queues.Graphics;
        rightVKTexData.m_nQueueFamilyIndex = handles->Queues.GraphicsFamilyIndex;
        rightVKTexData.m_nWidth = rightEye->GetWidth();
        rightVKTexData.m_nHeight = rightEye->GetHeight();
        rightVKTexData.m_nFormat = (VkFormat)rightEye->GetFormat();
        rightVKTexData.m_nSampleCount = 1;

        vr::VRTextureWithPose_t leftTex{};
        leftTex.handle = &leftVKTexData;
        leftTex.eColorSpace = vr::ColorSpace_Gamma;
        leftTex.eType = vr::TextureType_Vulkan;
        leftTex.mDeviceToAbsoluteTracking = fromMat4(usedPose);
        
        vr::VRCompositor()->Submit(vr::Eye_Left, &leftTex, nullptr, vr::Submit_TextureWithPose);

        vr::VRTextureWithPose_t rightTex{};
        rightTex.handle = &rightVKTexData;
        rightTex.eColorSpace = vr::ColorSpace_Gamma;
        rightTex.eType = vr::TextureType_Vulkan;
        rightTex.mDeviceToAbsoluteTracking = fromMat4(usedPose);

        vr::VRCompositor()->Submit(vr::Eye_Right, &rightTex, nullptr, vr::Submit_TextureWithPose);
    }
}