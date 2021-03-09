#pragma once
#include <iostream>
#include <glm/glm.hpp>

std::ostream& operator<<(std::ostream& os, const glm::vec3& vec) {
    os << '(' << vec.x << ',' << vec.y << ',' << vec.z << ')';
    return os;
}
