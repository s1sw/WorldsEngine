#pragma once
#include <cstdint>
#include <Libs/spirv_reflect.h>
#include <vector>

namespace R2::VK {
    class DescriptorSetLayout;
    class Core;
    class DescriptorSetUpdater;
    class Buffer;
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
        uint32_t getBindingIndex(const char* name);
        void bindBuffer(R2::VK::DescriptorSetUpdater& dsu, const char* bindPoint, R2::VK::Buffer* buffer);
        VertexAttributeBindings getVertexAttributeBindings();
    private:
        SpvReflectShaderModule mod;
        bool valid = true;
        std::vector<SpvReflectDescriptorBinding*> reflectBindings;
    };
}
