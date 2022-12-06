#pragma once
#include <glm/glm.hpp>
#include <iostream>

std::ostream& operator<<(std::ostream& os, const glm::vec3& vec)
{
    os << '(' << vec.x << ',' << vec.y << ',' << vec.z << ')';
    return os;
}
