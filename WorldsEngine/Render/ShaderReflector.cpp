#include "ShaderReflector.hpp"
#include <Core/Log.hpp>
#include <IO/IOUtil.hpp>
#include <R2/VKCore.hpp>
#include <R2/VKDescriptorSet.hpp>

using namespace R2;

namespace worlds
{
    ShaderReflector::ShaderReflector(AssetID shaderId)
    {
        int64_t fileLength;
        auto res = loadAssetToBuffer(shaderId, &fileLength);

        if (res.error != IOError::None)
        {
            logErr("Failed to load shader");
            valid = false;
            return;
        }

        SpvReflectResult result = spvReflectCreateShaderModule(fileLength, res.value, &mod);

        if (result != SPV_REFLECT_RESULT_SUCCESS)
        {
            logErr("Failed to create shader module");
            valid = false;
            return;
        }

        uint32_t numBindings;
        spvReflectEnumerateDescriptorBindings(&mod, &numBindings, nullptr);
        reflectBindings.resize(numBindings);
        spvReflectEnumerateDescriptorBindings(&mod, &numBindings, reflectBindings.data());
    }

    ShaderReflector::~ShaderReflector()
    {
        if (valid)
        {
            spvReflectDestroyShaderModule(&mod);
        }
    }

    R2::VK::DescriptorSetLayout* ShaderReflector::createDescriptorSetLayout(VK::Core* core, uint32_t setIndex)
    {
        bool useBindFlags = false;

        VK::ShaderStage stageFlags = VK::ShaderStage::Fragment | VK::ShaderStage::Vertex;

        if (mod.shader_stage == SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT ||
            mod.shader_stage == SPV_REFLECT_SHADER_STAGE_VERTEX_BIT)
        {
        }
        else if (mod.shader_stage == SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT)
        {
            stageFlags = VK::ShaderStage::Compute;
        }
        else
        {
            logErr(WELogCategoryRender, "Reflecting unsupported shader type!");
        }

        // Shaders might decide to alias descriptors so don't put them in multiple times
        std::vector<bool> presentBindings;
        int maxBinding = 0;

        for (auto binding : reflectBindings)
        {
            maxBinding = binding->binding > maxBinding ? binding->binding : maxBinding;
        }

        presentBindings.resize(maxBinding + 1);

        R2::VK::DescriptorSetLayoutBuilder dslb{core->GetHandles()};

        for (auto binding : reflectBindings)
        {
            if (binding->set != setIndex)
                continue;
            if (presentBindings[binding->binding])
                continue;
            presentBindings[binding->binding] = true;

            dslb.Binding(binding->binding, static_cast<VK::DescriptorType>(binding->descriptor_type), binding->count,
                         stageFlags);

            if (binding->count > 1)
            {
                dslb.PartiallyBound();
            }

            dslb.UpdateAfterBind();
        }

        return dslb.Build();
    }

    uint32_t ShaderReflector::getBindingIndex(const char* name)
    {
        for (SpvReflectDescriptorBinding* binding : reflectBindings)
        {
            if (strcmp(binding->name, name) == 0)
            {
                return binding->binding;
            }
        }

        return ~0u;
    }

    void ShaderReflector::bindBuffer(R2::VK::DescriptorSetUpdater& dsu, const char* bindPoint, R2::VK::Buffer* buffer)
    {
        for (SpvReflectDescriptorBinding* binding : reflectBindings)
        {
            if (strcmp(binding->name, bindPoint) == 0)
            {
                dsu.AddBuffer(binding->binding, 0, (VK::DescriptorType)binding->descriptor_type, buffer);
                return;
            }
        }
    }

    VertexAttributeBindings ShaderReflector::getVertexAttributeBindings()
    {
        VertexAttributeBindings bindings{-1, -1, -1, -1, -1, -1, -1};

        if (mod.shader_stage != SPV_REFLECT_SHADER_STAGE_VERTEX_BIT)
        {
            return bindings;
        }

        uint32_t numInputVariables;
        std::vector<SpvReflectInterfaceVariable*> inVars;
        spvReflectEnumerateInputVariables(&mod, &numInputVariables, nullptr);
        inVars.resize(numInputVariables);
        spvReflectEnumerateInputVariables(&mod, &numInputVariables, inVars.data());

        for (SpvReflectInterfaceVariable* iv : inVars)
        {
            if (strcmp(iv->name, "inPosition") == 0)
            {
                bindings.position = iv->location;
            }
            else if (strcmp(iv->name, "inNormal") == 0)
            {
                bindings.normal = iv->location;
            }
            else if (strcmp(iv->name, "inTangent") == 0)
            {
                bindings.tangent = iv->location;
            }
            else if (strcmp(iv->name, "inBitangentSign") == 0)
            {
                bindings.bitangentSign = iv->location;
            }
            else if (strcmp(iv->name, "inUV") == 0)
            {
                bindings.uv = iv->location;
            }
            else if (strcmp(iv->name, "inBoneWeights") == 0)
            {
                bindings.boneWeights = iv->location;
            }
            else if (strcmp(iv->name, "inBoneIds") == 0)
            {
                bindings.boneIds = iv->location;
            }
        }

        return bindings;
    }
}
