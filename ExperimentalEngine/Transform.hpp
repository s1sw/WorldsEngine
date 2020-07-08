#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

struct Transform {
	Transform() : position(0.0f), rotation() {}
	Transform(glm::vec3 position, glm::quat rotation) : position(position), rotation(rotation) {}
	glm::vec3 position;
	glm::quat rotation;

	inline glm::mat4 getMatrix() {
		return glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation);
	}
};