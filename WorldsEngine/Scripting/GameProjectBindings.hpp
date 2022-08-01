#include "Export.hpp"
#include <Editor/Editor.hpp>

extern "C"
{
    EXPORT void worlds__free(void* ptr)
    {
        free(ptr);
    }

    EXPORT char* worlds__GameProject_name(worlds::GameProject* inst)
    {
        return strdup(std::string((inst->name())).c_str());
    }

    EXPORT char* worlds__GameProject_root(worlds::GameProject* inst)
    {
        return strdup(std::string((inst->root())).c_str());
    }

    EXPORT char* worlds__GameProject_sourceData(worlds::GameProject* inst)
    {
        return strdup(std::string((inst->sourceData())).c_str());
    }

    EXPORT char* worlds__GameProject_builtData(worlds::GameProject* inst)
    {
        return strdup(std::string((inst->builtData())).c_str());
    }

    EXPORT char* worlds__GameProject_rawData(worlds::GameProject* inst)
    {
        return strdup(std::string((inst->rawData())).c_str());
    }
}
