#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#undef near
#undef far

struct Camera {
	Camera() 
		: position(0.0f)
		, rotation()
		, verticalFOV(1.25f)
		, near(0.01f)
		, far(2500.0f) {
	}
	glm::vec3 position;
	glm::quat rotation;
	float near;
	float far;
	float verticalFOV;

	glm::mat4 getViewMatrix() {
		return glm::lookAt(position, position + (rotation * glm::vec3(0.0f, 0.0f, 1.0f)), glm::vec3(0.0f, 1.0f, 0.0f));
	}

	glm::mat4 getProjectionMatrix(float aspect) {
		return glm::perspective(verticalFOV, aspect, near, far);
	}
};