#pragma once
#include <glm/glm.hpp>

namespace worlds {
	struct PackedMaterial {
		//glm::vec4 pack0;
		float metallic;
		float roughness;
		int albedoTexIdx;
		int normalTexIdx;
		//glm::vec4 pack1;
		glm::vec3 albedoColor;
		float alphaCutoff;

		float fresnelHackFactor;
		float pad0;
		float pad1;
		float pad2;
	};
}