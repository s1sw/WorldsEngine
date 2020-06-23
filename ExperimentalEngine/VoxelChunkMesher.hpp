#pragma once
#include "Engine.hpp"
#include "VoxelChunk.hpp"

class VoxelChunkMesher {
	void generateFace(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, glm::vec3 offset, glm::vec3 widthDir, glm::vec3 lengthDir, glm::vec3 normal) {
		// indices are always the same
		uint32_t idxOffset = vertices.size();
		vertices.push_back(Vertex{ offset, normal, glm::vec3(0.0f), glm::vec2(0.0f, 1.0f) });
		vertices.push_back(Vertex{ offset + lengthDir, normal, glm::vec3(0.0f), glm::vec2(1.0f, 1.0f) });
		vertices.push_back(Vertex{ offset + lengthDir + widthDir, normal, glm::vec3(0.0f), glm::vec2(1.0f, 0.0f) });
		vertices.push_back(Vertex{ offset + widthDir, normal, glm::vec3(0.0f), glm::vec2(0.0f, 0.0f) });

		indices.push_back(idxOffset + 0);
		indices.push_back(idxOffset + 1);
		indices.push_back(idxOffset + 2);
		indices.push_back(idxOffset + 0);
		indices.push_back(idxOffset + 2);
		indices.push_back(idxOffset + 3);
	}

	bool isVoxelAt(VoxelChunk& vc, int x, int y, int z) {
		if (x < 0 || x > 15) return false;
		if (y < 0 || y > 15) return false;
		if (z < 0 || z > 15) return false;

		return vc.data[x][y][z] != 0;
	}

	void genBlockFaces(VoxelChunk& voxelChunk, glm::vec3 pos, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) {
		//if (!isVoxelAt(voxelChunk, pos.x, pos.y, pos.z)) return;
		if (voxelChunk.data[(int)pos.x][(int)pos.y][(int)pos.z] == 0) return;
		glm::vec3 up(0.0f, 1.0f, 0.0f);
		glm::vec3 down(0.0f, -1.0f, 0.0f);
		glm::vec3 left(-1.0f, 0.0f, 0.0f);
		glm::vec3 right(1.0f, 0.0f, 0.0f);
		glm::vec3 forward(0.0f, 0.0f, 1.0f);
		glm::vec3 backward(0.0f, 0.0f, -1.0f);

		glm::vec3 nearCorner = pos;
		glm::vec3 farCorner = pos + glm::vec3(1.0f, 1.0f, 1.0f);

		// Top Face
		if (!isVoxelAt(voxelChunk, pos.x, pos.y + 1, pos.z)) {
			generateFace(vertices, indices, farCorner, left, backward, up);
		}

		// Bottom Face
		if (!isVoxelAt(voxelChunk, pos.x, pos.y - 1, pos.z))
			generateFace(vertices, indices, nearCorner, forward, right, down);

		// Left Face
		if (!isVoxelAt(voxelChunk, pos.x - 1, pos.y, pos.z))
			generateFace(vertices, indices, nearCorner, up, forward, left);

		// Right Face
		if (!isVoxelAt(voxelChunk, pos.x + 1, pos.y, pos.z))
			generateFace(vertices, indices, farCorner, backward, down, right);

		// Front Face
		if (!isVoxelAt(voxelChunk, pos.x, pos.y, pos.z - 1))
			generateFace(vertices, indices, nearCorner, right, up, forward);

		// Back Face
		if (!isVoxelAt(voxelChunk, pos.x, pos.y, pos.z + 1))
			generateFace(vertices, indices, farCorner, down, left, backward);
	}

public:
	void updateVoxelChunk(VoxelChunk& voxelChunk, ProceduralObject& procObj) {
		if (!voxelChunk.dirty) return;

		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		for (int x = 0; x < 16; x++)
			for (int y = 0; y < 16; y++)
				for (int z = 0; z < 16; z++) {
					glm::vec3 pos(x, y, z);
					genBlockFaces(voxelChunk, pos, vertices, indices);
				}

		procObj.vertices = std::move(vertices);
		procObj.indices = std::move(indices);
		procObj.uploaded = false;
		procObj.readyForUpload = true;
	}
};