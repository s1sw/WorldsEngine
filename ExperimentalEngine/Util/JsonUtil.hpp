#pragma once
#include "sajson.h"
#include <glm/glm.hpp>

namespace worlds {
    inline void getVec3(const sajson::value& obj, const char* key, glm::vec3& val) {
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

    inline void getQuat(const sajson::value& obj, const char* key, glm::quat& val) {
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

    inline void getFloat(const sajson::value& obj, const char* key, float& val) {
        sajson::string keyStr{ key, strlen(key) };

        auto idx = obj.find_object_key(keyStr);

        if (idx == obj.get_length())
            return;

        val = obj.get_object_value(idx).get_double_value();
    }

    inline bool hasKey(const sajson::value& obj, const char* key) {
        sajson::string keyStr{ key, strlen(key) };

        auto idx = obj.find_object_key(keyStr);

        return idx != obj.get_length();
    }
}