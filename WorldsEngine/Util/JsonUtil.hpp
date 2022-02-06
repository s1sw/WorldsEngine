#pragma once
#include <glm/glm.hpp>
#include <slib/String.hpp>
#include <nlohmann/json.hpp>

namespace slib {
    void to_json(nlohmann::json& j, const slib::String& str);
    void from_json(const nlohmann::json& j, slib::String& str);
}

namespace worlds {
    struct EntityFolder;
    void to_json(nlohmann::json& j, const EntityFolder& folder);
    void from_json(const nlohmann::json& j, EntityFolder& folder);
}

namespace glm {
    void to_json(nlohmann::json& j, const glm::vec3& vec);
    void from_json(const nlohmann::json& j, glm::vec3& vec);

    void to_json(nlohmann::json& j, const glm::quat& q);
    void from_json(const nlohmann::json& j, glm::quat& q);

    void to_json(nlohmann::json& j, const glm::vec4& vec);
    void from_json(const nlohmann::json& j, glm::vec4& vec);
}
