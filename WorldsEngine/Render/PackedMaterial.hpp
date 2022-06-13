#pragma once
#include <glm/glm.hpp>

namespace worlds {
	enum class MaterialFlags {
		None = 0,
		UsePackedPBR = 1,
		UsePackedHeightmap = 2,
		UseAlbedoEmissive = 4
	};

	inline MaterialFlags operator | (MaterialFlags lhs, MaterialFlags rhs) {
		return static_cast<MaterialFlags>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
	}

	struct PackedMaterial {
		float metallic;
		float roughness;
		int albedoTexIdx;
		int normalTexIdx;

		glm::vec3 albedoColor;
		uint32_t cutoffFlags = 0u;

		int heightmapTexIdx;
		float heightmapScale;
		int metalTexIdx;
		int roughTexIdx;

        glm::vec3 emissiveColor;
		int aoTexIdx;

		float getCutoff() {
			return (cutoffFlags & 0xFF) / 255.0f;
		}

		void setCutoff(float cutoff) {
			uint32_t flags = cutoffFlags >> 8;
			cutoff = glm::clamp(cutoff, 0.0f, 1.0f);
			cutoffFlags = (flags << 8) | (uint32_t)(cutoff * 255);
		}

		void setFlags(MaterialFlags flags) {
			uint32_t cutoff = (cutoffFlags & 0xFF);
			uint32_t flag = (uint32_t)(flags) << 8;
			cutoffFlags = cutoff | flag;
		}

		MaterialFlags getFlags() {
			return (MaterialFlags)((cutoffFlags & (0x7FFFFF80)) >> 8);
		}
	};
}
