#pragma once
#include <cstdint>
#include <Render/vku/vku.hpp>
#include <Libs/spirv_reflect.h>

namespace worlds {
    typedef uint32_t AssetID;

    class ShaderReflector {
    public:
        ShaderReflector(AssetID shaderId);
        ~ShaderReflector();
        vku::DescriptorSetLayout createDescriptorSetLayout(VkDevice device, int setIndex);
    private:
        SpvReflectShaderModule mod;
        bool valid = true;
    };
}
