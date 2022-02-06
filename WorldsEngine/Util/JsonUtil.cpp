#include "JsonUtil.hpp"
#include <nlohmann/json.hpp>
#include <glm/gtc/quaternion.hpp>

using json = nlohmann::json;

namespace slib {
    void to_json(json& j, const slib::String& str) {
        j = std::string(str.cStr());
    }

    void from_json(const json& j, slib::String& str) {
        str = slib::String{j.get<std::string>().c_str()};
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
