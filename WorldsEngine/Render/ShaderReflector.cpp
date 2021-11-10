#include "ShaderReflector.hpp"
#include <IO/IOUtil.hpp>
#include <Render/vku/DescriptorSetUtil.hpp>

namespace worlds {
    ShaderReflector::ShaderReflector(AssetID shaderId) {
        int64_t fileLength;
        auto res = loadAssetToBuffer(shaderId, &fileLength);

        if (res.error != IOError::None) {
            logErr("Failed to load shader");
            valid = false;
            return;
        }

        SpvReflectResult result = spvReflectCreateShaderModule(fileLength, res.value, &mod);

        if (result != SPV_REFLECT_RESULT_SUCCESS) {
            logErr("Failed to create shader module");
            valid = false;
            return;
        }
    }

    ShaderReflector::~ShaderReflector() {
        if (valid) {
            spvReflectDestroyShaderModule(&mod);
        }
    }

    vku::DescriptorSetLayout ShaderReflector::createDescriptorSetLayout(VkDevice device, int setIndex) {
        uint32_t numBindings;
        std::vector<SpvReflectDescriptorBinding*> bindings;
        spvReflectEnumerateDescriptorBindings(&mod, &numBindings, nullptr);
        bindings.resize(numBindings);
        spvReflectEnumerateDescriptorBindings(&mod, &numBindings, bindings.data());

        bool useBindFlags = false;
        std::vector<VkDescriptorSetLayoutBinding> dslBindings;
        std::vector<VkDescriptorBindingFlags> bindFlags;

        VkShaderStageFlags stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;

        if (mod.shader_stage == SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT || mod.shader_stage == SPV_REFLECT_SHADER_STAGE_VERTEX_BIT) {
        } else if (mod.shader_stage == SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT) {
            stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        } else {
            logErr(WELogCategoryRender, "Reflecting unsupported shader type!");
        }

        for (auto binding : bindings) {
            if (binding->set != setIndex) continue;

            VkDescriptorSetLayoutBinding vkBinding{
                .binding = binding->binding,
                .descriptorType = (VkDescriptorType)binding->descriptor_type,
                .descriptorCount = binding->count,
                .stageFlags = stageFlags,
                .pImmutableSamplers = nullptr
            };

            if (binding->count > 1) {
                bindFlags.push_back(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
                useBindFlags = true;
            } else {
                bindFlags.push_back({});
            }

            dslBindings.push_back(vkBinding);
        }

        assert(bindFlags.size() == dslBindings.size());

        VkDescriptorSetLayoutCreateInfo dslci { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        dslci.bindingCount = dslBindings.size();
        dslci.pBindings = dslBindings.data();

        VkDescriptorSetLayoutBindingFlagsCreateInfo dslbfci{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO };
        if (useBindFlags) {
            dslbfci.bindingCount = bindFlags.size();
            dslbfci.pBindingFlags = bindFlags.data();
            dslci.pNext = &dslbfci;
        }

        VkDescriptorSetLayout layout;
        VKCHECK(vkCreateDescriptorSetLayout(device, &dslci, nullptr, &layout));

        return layout;
    }
}
