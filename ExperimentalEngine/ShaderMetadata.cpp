#include "ShaderMetadata.hpp"
#include <spirv/1.2/spirv.h>
#include "spirv_reflect.h"
#include <spirv/1.2/spirv.hpp>
#include <vector>
#include <iostream>
#include <cassert>
#include <unordered_map>

template <typename T, typename U>
bool enumHasFlag(T en, U flag) {
    return (en & flag) == flag;
}

bool validateLightBuffer(SpvReflectTypeDescription* typeDesc) {
    if (typeDesc->member_count != 3) return false;

    bool membersValid = true;
    membersValid &= enumHasFlag(typeDesc->members[0].type_flags, SPV_REFLECT_TYPE_FLAG_VECTOR | SPV_REFLECT_TYPE_FLAG_FLOAT);
    membersValid &= typeDesc->members[0].traits.numeric.vector.component_count == 4;
    membersValid &= enumHasFlag(typeDesc->members[1].type_flags, SPV_REFLECT_TYPE_FLAG_MATRIX | SPV_REFLECT_TYPE_FLAG_FLOAT);
    membersValid &= typeDesc->members[1].traits.numeric.matrix.column_count == 4;
    membersValid &= typeDesc->members[1].traits.numeric.matrix.row_count == 4;
    membersValid &= enumHasFlag(typeDesc->members[2].type_flags, SPV_REFLECT_TYPE_FLAG_STRUCT | SPV_REFLECT_TYPE_FLAG_ARRAY);

    return membersValid;
}

bool validateModelMatrixBuffer(SpvReflectTypeDescription* typeDesc) {
    if (typeDesc->member_count != 1) return false;

    auto tFlags = typeDesc->members[0].type_flags;
    bool valid = true;
    valid &= enumHasFlag(tFlags, SPV_REFLECT_TYPE_FLAG_ARRAY | SPV_REFLECT_TYPE_FLAG_MATRIX);
    valid &= typeDesc->members[0].traits.numeric.matrix.column_count == 4;
    valid &= typeDesc->members[0].traits.numeric.matrix.row_count == 4;
    
    return valid;
}

ShaderMetadata generateSpirvMetadata(uint32_t* data, size_t length) {
    ShaderMetadata metadata{};
    metadata.valid = true;

    SpvReflectShaderModule fsRefl;
    auto res1 = spvReflectCreateShaderModule(length, data, &fsRefl);

    if (res1 != SPV_REFLECT_RESULT_SUCCESS) return ShaderMetadata{ .valid = false };

    if (fsRefl.source_file)
        metadata.sourceFile = fsRefl.source_file;
    else
        metadata.sourceFile = "unknown";

    uint32_t inputVarCount;
    spvReflectEnumerateInputVariables(&fsRefl, &inputVarCount, nullptr);

    std::vector<SpvReflectInterfaceVariable*> inputVars(inputVarCount);
    spvReflectEnumerateInputVariables(&fsRefl, &inputVarCount, inputVars.data());

    uint32_t outputVarCount;
    spvReflectEnumerateOutputVariables(&fsRefl, &outputVarCount, nullptr);

    std::vector<SpvReflectInterfaceVariable*> outputVars(outputVarCount);
    spvReflectEnumerateOutputVariables(&fsRefl, &outputVarCount, outputVars.data());

    for (uint32_t i = 0; i < fsRefl.descriptor_sets[0].binding_count; i++) {
        auto* typeDesc = fsRefl.descriptor_sets[0].bindings[i]->type_description;
        if (typeDesc->op == SpvOpTypeStruct) {
            if (strcmp(typeDesc->type_name, "ModelMatrices") == 0) {
                metadata.modelMatrixBinding = fsRefl.descriptor_sets[0].bindings[i]->binding;

                bool valid = validateModelMatrixBuffer(typeDesc);

                if (!valid) {
                    std::cerr << "Model matrix buffer failed validation\n";
                }

                metadata.valid &= valid;
            }

            if (strcmp(typeDesc->type_name, "LightBuffer") == 0) {
                metadata.lightBufferBinding = fsRefl.descriptor_sets[0].bindings[i]->binding;

                // Ensure that the light buffer layout is correct
                bool valid = validateLightBuffer(typeDesc);

                if (!valid) {
                    std::cerr << "Light buffer failed validation\n";
                }

                metadata.valid &= valid;
            }
        }
    }

    if (fsRefl.push_constant_block_count > 0) {
        for (uint32_t j = 0; j < fsRefl.push_constant_blocks[0].member_count; j++) {
            auto m = fsRefl.push_constant_blocks[0].members[j].type_description;

            if (strcmp(m->struct_member_name, "viewPos") && enumHasFlag(m->type_flags, SPV_REFLECT_TYPE_FLAG_VECTOR)) {
                metadata.viewPosPCOffset = fsRefl.push_constant_blocks[0].members[j].offset;
            }

            if (strcmp(m->struct_member_name, "texScaleOffset")) {
                metadata.texScaleOffsetPCOffset = fsRefl.push_constant_blocks[0].members[j].offset;
            }

            if (strcmp(m->struct_member_name, "ubIndices")) {
                metadata.ubIndicesPCOffset = fsRefl.push_constant_blocks[0].members[j].offset;
            }
        }
    }

    std::cout << "Input vars:\n";
    for (auto* var : inputVars) {
        if (var->location == 4294967295)
            continue;

        ShaderVar sVar{};
        sVar.name = var->name;
        sVar.location = var->location;
        sVar.bitWidth = var->type_description->traits.numeric.scalar.width;
        sVar.vectorWidth = var->type_description->traits.numeric.vector.component_count;
        metadata.inputVars.emplace_back(sVar);
        std::cout << var->name << ": " << var->location << "\n";
    }

    std::cout << "Output vars:\n";
    for (auto* var : outputVars) {
        if (var->location == 4294967295)
            continue;

        ShaderVar sVar{};
        sVar.name = var->name;
        sVar.location = var->location;
        sVar.bitWidth = var->type_description->traits.numeric.scalar.width;
        sVar.vectorWidth = var->type_description->traits.numeric.vector.component_count;
        metadata.outputVars.emplace_back(sVar);
        std::cout << var->name << ": " << var->location << "\n";
    }

    spvReflectDestroyShaderModule(&fsRefl);

    return metadata;
}

bool shadersCompatible(ShaderMetadata& vs, ShaderMetadata& fs) {
    if (!fs.valid || !vs.valid) return false;

    if (vs.lightBufferBinding.has_value() && 
        fs.lightBufferBinding.has_value() && 
        vs.lightBufferBinding != fs.lightBufferBinding)
        return false;

    if (vs.modelMatrixBinding.has_value() &&
        fs.modelMatrixBinding.has_value() &&
        vs.modelMatrixBinding != fs.modelMatrixBinding)
        return false;

    std::unordered_map<uint8_t, ShaderVar> vsOutVars;
    vsOutVars.reserve(vs.outputVars.size());

    for (auto& outVar : vs.outputVars) {
        vsOutVars.insert({ outVar.location, outVar });
    }

    for (auto& inVar : fs.inputVars) {
        auto outVarIt = vsOutVars.find(inVar.location);
        if (outVarIt == vsOutVars.end()) return false;

        if ((*outVarIt).second != inVar) return false;
    }

    return true;
}
