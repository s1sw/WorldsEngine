#include "ShaderMetadata.hpp"

ShaderMetadata generateSpirvMetadata(uint32_t* data, size_t length) {
    SpvReflectShaderModule fsRefl;
    auto res1 = spvReflectCreateShaderModule(fsLen, fsData, &fsRefl);

    uint32_t inputVarCount;
    spvReflectEnumerateInputVariables(&fsRefl, &inputVarCount, nullptr);

    std::vector<SpvReflectInterfaceVariable*> inputVars(inputVarCount);
    assert(inputVars.size() == inputVarCount);
    spvReflectEnumerateInputVariables(&fsRefl, &inputVarCount, inputVars.data());

    for (int i = 0; i < fsRefl.descriptor_sets[0].binding_count; i++) {
        auto* typeDesc = fsRefl.descriptor_sets[0].bindings[i]->type_description;
        if (typeDesc->op == SpvOpTypeStruct) {
            std::cout << "Uniform buffer " << typeDesc->type_name << " has " << typeDesc->member_count << " members:\n";

            for (int j = 0; j < typeDesc->member_count; j++) {
                auto m = typeDesc->members[j];
                std::cout << "\t";
                if ((m.type_flags & SPV_REFLECT_TYPE_FLAG_VECTOR) == SPV_REFLECT_TYPE_FLAG_VECTOR)
                    std::cout << "vec" << m.traits.numeric.vector.component_count;
                else if ((m.type_flags & SPV_REFLECT_TYPE_FLAG_STRUCT) == SPV_REFLECT_TYPE_FLAG_STRUCT) {
                    std::cout << m.type_name;
                } else if ((m.type_flags & SPV_REFLECT_TYPE_FLAG_INT) == SPV_REFLECT_TYPE_FLAG_INT) {
                    if (!m.traits.numeric.scalar.signedness)
                        std::cout << "u";
                    std::cout << "int";
                }

                std::cout << " " << m.struct_member_name;

                if ((m.type_flags & SPV_REFLECT_TYPE_FLAG_ARRAY) == SPV_REFLECT_TYPE_FLAG_ARRAY)
                    std::cout << "[" << m.traits.array.dims[0] << "]";

                std::cout << "\n";
            }
        }
    }

    for (auto* var : inputVars) {
        std::cout << var->name << ": " << var->location << "\n";
    }

}