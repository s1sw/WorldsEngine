#include "RenderPasses.hpp"

PolyRenderPass::PolyRenderPass(
    RenderImageHandle depthStencilImage,
    RenderImageHandle polyImage,
    RenderImageHandle shadowImage)
    : depthStencilImage(depthStencilImage)
    , polyImage(polyImage)
    , shadowImage(shadowImage) {

}