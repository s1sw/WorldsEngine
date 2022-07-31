#include <Render/StandardPipeline/Tonemapper.hpp>

namespace worlds
{
    Tonemapper::Tonemapper(R2::VK::Texture* colorBuffer, R2::VK::Texture* target)
        : colorBuffer(colorBuffer)
        , target(target)
    {
    }
}