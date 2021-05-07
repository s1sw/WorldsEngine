#include "JsonUtil.hpp"
#include <nlohmann/json.hpp>
#include <glm/gtc/quaternion.hpp>

using json = nlohmann::json;

namespace worlds {
    void getVec3(const sajson::value& obj, const char* key, glm::vec3& val) {
        sajson::string keyStr{ key, strlen(key) };

        auto idx = obj.find_object_key(keyStr);

        if (idx == obj.get_length())
            return;

        const auto& arr = obj.get_object_value(idx);

        glm::vec3 vec{
            arr.get_array_element(0).get_double_value(),
            arr.get_array_element(1).get_double_value(),
            arr.get_array_element(2).get_double_value()
        };

        val = vec;
    }

    void getQuat(const sajson::value& obj, const char* key, glm::quat& val) {
        sajson::string keyStr{ key, strlen(key) };

        auto idx = obj.find_object_key(keyStr);

        if (idx == obj.get_length())
            return;

        const auto& arr = obj.get_object_value(idx);

        // Our JSON files use XYZW
        // GLM uses WXYZ for this constructor
        glm::quat q{
            (float)arr.get_array_element(3).get_double_value(),
            (float)arr.get_array_element(0).get_double_value(),
            (float)arr.get_array_element(1).get_double_value(),
            (float)arr.get_array_element(2).get_double_value()
        };

        val = q;
    }

    void getFloat(const sajson::value& obj, const char* key, float& val) {
        sajson::string keyStr{ key, strlen(key) };

        auto idx = obj.find_object_key(keyStr);

        if (idx == obj.get_length())
            return;

        val = obj.get_object_value(idx).get_double_value();
    }

    bool hasKey(const sajson::value& obj, const char* key) {
        sajson::string keyStr{ key, strlen(key) };

        auto idx = obj.find_object_key(keyStr);

        return idx != obj.get_length();
    }
}

namespace glm {
    // glm::vec3
    void to_json(json& j, const glm::vec3& vec) {
        j = nlohmann::json{vec.x, vec.y, vec.z};
    }

    void from_json(const json& j, glm::vec3& vec) {
        vec = glm::vec3{j[0], j[1], j[2]};
    }

    // glm::quat
    void to_json(json& j, const glm::quat& q) {
        j = nlohmann::json { q.x, q.y, q.z, q.w };
    }

    void from_json(const json& j, glm::quat& q) {
        // This constructor uses WXYZ but quaternions are stored
        // in XYZW order
        q = glm::quat { j[3], j[0], j[1], j[2] };
    }

    // glm::vec4
    void to_json(json& j, const glm::vec4& vec) {
        j = { vec.x, vec.y, vec.z, vec.w };
    }

    void from_json(const json& j, glm::vec4& vec) {
        vec = glm::vec4 { j[0], j[1], j[2], j[3] };
    }
}
