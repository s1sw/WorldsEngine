#pragma once
#include <glm/glm.hpp>

namespace worlds {
	struct PackedMaterial {
		float metallic;
		float roughness;
		int albedoTexIdx;
		int normalTexIdx;

		glm::vec3 albedoColor;
		float alphaCutoff;

		int heightmapTexIdx;
		float heightmapScale;
		int metalTexIdx;
		int roughTexIdx;

        glm::vec3 emissiveColor;
        float pad3;
	};
}
