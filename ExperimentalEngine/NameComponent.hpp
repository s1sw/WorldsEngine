#pragma once
#include "Log.hpp"
#include <entt/entt.hpp>
#include <limits>
#include <physfs.h>
#include <string>

namespace worlds {
    struct NameComponent {
        std::string name;

        static void save(entt::entity ent, entt::registry& reg, PHYSFS_File* file) {
            auto& nc = reg.get<NameComponent>(ent);
            
            if (nc.name.size() > std::numeric_limits<uint16_t>::max()) {
                logWarn("object name is too long (>65536 chars)! saving it truncated...");
                nc.name = nc.name.substr(0, std::numeric_limits<uint16_t>::max());
            }

            PHYSFS_writeULE16(file, nc.name.size());
            PHYSFS_writeBytes(file, nc.name.data(), nc.name.size());
        }

        static void load(entt::entity ent, entt::registry& reg, PHYSFS_File* file) {
            auto& nc = reg.get_or_emplace<NameComponent>(ent);

            uint16_t size;
            PHYSFS_readULE16(file, &size);

            nc.name.resize(size);
            PHYSFS_readBytes(file, nc.name.data(), size);
        }
    };
}
