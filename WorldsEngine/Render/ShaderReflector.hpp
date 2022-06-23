#pragma once
#include <cstdint>
#include <Libs/spirv_reflect.h>

namespace R2::VK {
    class DescriptorSetLayout;
    class Core;
}

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
        R2::VK::DescriptorSetLayout* createDescriptorSetLayout(R2::VK::Core* device, uint32_t setIndex);
        VertexAttributeBindings getVertexAttributeBindings();
    private:
        SpvReflectShaderModule mod;
        bool valid = true;
    };
}
