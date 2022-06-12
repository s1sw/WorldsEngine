#include "Export.hpp"
#include <Editor/Editor.hpp>
#include <Physics/PhysicsActor.hpp>

EXPORT char* worlds__GameProject_name(worlds::GameProject* inst) {
    return strdup(std::string((inst->name())).c_str());
}

EXPORT char* worlds__GameProject_root(worlds::GameProject* inst) {
    return strdup(std::string((inst->root())).c_str());
}

EXPORT char* worlds__GameProject_sourceData(worlds::GameProject* inst) {
    return strdup(std::string((inst->sourceData())).c_str());
}

EXPORT char* worlds__GameProject_builtData(worlds::GameProject* inst) {
    return strdup(std::string((inst->builtData())).c_str());
}

EXPORT char* worlds__GameProject_rawData(worlds::GameProject* inst) {
    return strdup(std::string((inst->rawData())).c_str());
}

EXPORT bool worlds__RigidBody_enabled_get(entt::registry* reg, entt::entity entity) {
    return (reg->get<worlds::RigidBody>(entity))->enabled;
}

