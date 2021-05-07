#pragma once
#include "sajson.h"
#include <glm/glm.hpp>
#include <nlohmann/json_fwd.hpp>

namespace worlds {
    void getVec3(const sajson::value& obj, const char* key, glm::vec3& val);
    void getQuat(const sajson::value& obj, const char* key, glm::quat& val);
    void getFloat(const sajson::value& obj, const char* key, float& val);
    bool hasKey(const sajson::value& obj, const char* key);
}

namespace glm {
    void to_json(nlohmann::json& j, const glm::vec3& vec);
    void from_json(const nlohmann::json& j, glm::vec3& vec);

    void to_json(nlohmann::json& j, const glm::quat& q);
    void from_json(const nlohmann::json& j, glm::quat& q);

    void to_json(nlohmann::json& j, const glm::vec4& vec);
    void from_json(const nlohmann::json& j, glm::vec4& vec);
}
