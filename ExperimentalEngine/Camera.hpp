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
		, near(0.01f)
		, far(2500.0f)
		, verticalFOV(1.25f) {
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
		return reverseZInfPerspective(verticalFOV, aspect, near);
	}

	// returns the non-reverse Z projection matrix
	glm::mat4 getProjectionMatrixZO(float aspect) {
		return glm::infinitePerspective(verticalFOV, aspect, near);
	}

private:
	glm::mat4 reverseZInfPerspective(float verticalFOV, float aspect, float zNear) {
		float f = 1.0f / tan(verticalFOV / 2.0f);
		return glm::mat4(
			f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.0f, -1.0f,
			0.0f, 0.0f, zNear, 0.0f);
	}
};