#include <Render/RenderInternal.hpp>
#include <Core/Engine.hpp>
#include <R2/VK.hpp>
#include <VR/OpenXRInterface.hpp>

using namespace R2;

namespace worlds
{
    XRPresentManager::XRPresentManager(VKRenderer* renderer, const EngineInterfaces& interfaces, int width, int height)
        : width(width)
        , height(height)
        , interfaces(interfaces)
        , core(renderer->getCore())
    {
    }

    void XRPresentManager::copyFromLayered(VK::CommandBuffer cb, VK::Texture* layeredTexture)
    {
        interfaces.vrInterface->submitLayered(cb, layeredTexture);
    }

    void XRPresentManager::beginFrame()
    {
        interfaces.vrInterface->beginFrame();
    }

    void XRPresentManager::waitFrame()
    {
        interfaces.vrInterface->waitFrame();
    }

    void XRPresentManager::endFrame()
    {
        interfaces.vrInterface->endFrame();
    }
}