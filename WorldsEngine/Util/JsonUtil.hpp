#pragma once
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

namespace glm {
    void to_json(nlohmann::json& j, const glm::vec3& vec);
    void from_json(const nlohmann::json& j, glm::vec3& vec);

    void to_json(nlohmann::json& j, const glm::quat& q);
    void from_json(const nlohmann::json& j, glm::quat& q);

    void to_json(nlohmann::json& j, const glm::vec4& vec);
    void from_json(const nlohmann::json& j, glm::vec4& vec);
}
