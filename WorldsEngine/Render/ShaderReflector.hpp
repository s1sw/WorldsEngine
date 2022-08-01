#pragma once
#include <Libs/spirv_reflect.h>
#include <cstdint>
#include <vector>

namespace R2::VK
{
    class DescriptorSetLayout;
    class Core;
    class DescriptorSetUpdater;
    class Buffer;
    class Texture;
    class Sampler;
    struct VertexBinding;

    enum class TextureFormat;
}

namespace worlds
{
    typedef uint32_t AssetID;

    struct VertexAttributeBindings
    {
        int position;
        int normal;
        int tangent;
        int bitangentSign;
        int uv;
        int boneWeights;
        int boneIds;
    };

    class ShaderReflector
    {
      public:
        ShaderReflector(AssetID shaderId);
        ~ShaderReflector();
        R2::VK::DescriptorSetLayout* createDescriptorSetLayout(R2::VK::Core* device, uint32_t setIndex);
        uint32_t getBindingIndex(const char* name);
        void bindBuffer(R2::VK::DescriptorSetUpdater& dsu, const char* bindPoint, R2::VK::Buffer* buffer);
        void bindSampledTexture(R2::VK::DescriptorSetUpdater& dsu, const char* bindPoint, R2::VK::Texture* texture, R2::VK::Sampler* sampler);
        void bindStorageTexture(R2::VK::DescriptorSetUpdater& dsu, const char* bindPoint, R2::VK::Texture* texture);
        void bindVertexAttribute(R2::VK::VertexBinding& vb, const char* attribute, R2::VK::TextureFormat format, uint32_t offset);
        bool usesPushConstants();
        uint32_t getPushConstantsSize();
        [[deprecated]] VertexAttributeBindings getVertexAttributeBindings();

      private:
        SpvReflectShaderModule mod;
        bool valid = true;
        std::vector<SpvReflectDescriptorBinding*> reflectBindings;
        std::vector<SpvReflectInterfaceVariable*> interfaceVars;
    };
}
