#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

struct Transform {
	Transform() : position(0.0f), rotation(1.0f, 0.0f, 0.0f, 0.0f), scale(1.0f) {}
	Transform(glm::vec3 position, glm::quat rotation) : position(position), rotation(rotation), scale(1.0f) {}
	glm::vec3 position;
	glm::quat rotation;
	glm::vec3 scale;

	inline glm::mat4 getMatrix() const {
		return glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);
	}

	inline void fromMatrix(glm::mat4 mat) {
		glm::vec3 skew;
		glm::vec4 perspective;
		glm::decompose(mat, scale, rotation, position, skew, perspective);
	}
};