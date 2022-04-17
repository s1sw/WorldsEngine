#pragma once
#include <cstdint>
#include <Render/vku/vku.hpp>
#include <Libs/spirv_reflect.h>

namespace worlds {
    typedef uint32_t AssetID;

    struct VertexAttributeBindings {
        int position;
        int normal;
        int tangent;
        int bitangentSign;
        int uv;
        int boneWeights;
        int boneIds;
    };

    class ShaderReflector {
    public:
        ShaderReflector(AssetID shaderId);
        ~ShaderReflector();
        vku::DescriptorSetLayout createDescriptorSetLayout(VkDevice device, uint32_t setIndex);
        VertexAttributeBindings getVertexAttributeBindings();
    private:
        SpvReflectShaderModule mod;
        bool valid = true;
    };
}
