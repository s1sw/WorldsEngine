#pragma once
#include "Engine.hpp"
#include "VoxelChunk.hpp"

class VoxelChunkMesher {
	void generateFace(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, glm::vec3 offset, glm::vec3 widthDir, glm::vec3 lengthDir, glm::vec3 normal, glm::vec4 vertAO) {
		// indices are always the same
		uint32_t idxOffset = (uint32_t)vertices.size();
		vertices.push_back(Vertex{ offset, normal, glm::vec3(0.0f), glm::vec2(0.0f, 1.0f), vertAO.x });
		vertices.push_back(Vertex{ offset + lengthDir, normal, glm::vec3(0.0f), glm::vec2(1.0f, 1.0f), vertAO.y });
		vertices.push_back(Vertex{ offset + lengthDir + widthDir, normal, glm::vec3(0.0f), glm::vec2(1.0f, 0.0f), vertAO.z });
		vertices.push_back(Vertex{ offset + widthDir, normal, glm::vec3(0.0f), glm::vec2(0.0f, 0.0f), vertAO.w });

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

	bool isVoxelAt(VoxelChunk& vc, glm::vec3 pos) {
		return isVoxelAt(vc, pos.x, pos.y, pos.z);
	}

	float vertexAO(bool side1, bool side2, bool corner) {
		if (side1 && side2) return 0.f;

		// bools are just spicy ints
		return 3.0f - (side1 + side2 + corner);
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
		glm::ivec3 iPos{ pos };

		glm::vec4 ao(1.0f);
		// Top Face
		if (!isVoxelAt(voxelChunk, iPos.x, iPos.y + 1, iPos.z)) {
			glm::vec3 norm = up;

			// front right
			ao.x = vertexAO(
				isVoxelAt(voxelChunk, iPos.x + 1, iPos.y + 1, iPos.z), 
				isVoxelAt(voxelChunk, iPos.x, iPos.y + 1, iPos.z + 1), 
				isVoxelAt(voxelChunk, iPos.x + 1, iPos.y + 1, iPos.z + 1));

			// front left
			ao.w = vertexAO(
				isVoxelAt(voxelChunk, iPos.x - 1, iPos.y + 1, iPos.z),
				isVoxelAt(voxelChunk, iPos.x, iPos.y + 1, iPos.z + 1),
				isVoxelAt(voxelChunk, iPos.x - 1, iPos.y + 1, iPos.z + 1));

			// back left
			ao.z = vertexAO(
				isVoxelAt(voxelChunk, iPos.x - 1, iPos.y + 1, iPos.z),
				isVoxelAt(voxelChunk, iPos.x, iPos.y + 1, iPos.z - 1),
				isVoxelAt(voxelChunk, iPos.x - 1, iPos.y + 1, iPos.z - 1));

			// back right
			ao.y = vertexAO(
				isVoxelAt(voxelChunk, iPos.x + 1, iPos.y + 1, iPos.z),
				isVoxelAt(voxelChunk, iPos.x, iPos.y + 1, iPos.z - 1),
				isVoxelAt(voxelChunk, iPos.x + 1, iPos.y + 1, iPos.z - 1));

			generateFace(vertices, indices, farCorner, left, backward, up, ao * 0.33f);
		}

		// Bottom Face
		if (!isVoxelAt(voxelChunk, iPos.x, iPos.y - 1, iPos.z)) {
			// front right
			ao.z = vertexAO(
				isVoxelAt(voxelChunk, iPos.x + 1, iPos.y - 1, iPos.z),
				isVoxelAt(voxelChunk, iPos.x, iPos.y - 1, iPos.z + 1),
				isVoxelAt(voxelChunk, iPos.x + 1, iPos.y - 1, iPos.z + 1));

			// front left
			ao.w = vertexAO(
				isVoxelAt(voxelChunk, iPos.x - 1, iPos.y - 1, iPos.z),
				isVoxelAt(voxelChunk, iPos.x, iPos.y - 1, iPos.z + 1),
				isVoxelAt(voxelChunk, iPos.x - 1, iPos.y - 1, iPos.z + 1));

			// back left
			ao.x = vertexAO(
				isVoxelAt(voxelChunk, iPos.x - 1, iPos.y - 1, iPos.z),
				isVoxelAt(voxelChunk, iPos.x, iPos.y - 1, iPos.z - 1),
				isVoxelAt(voxelChunk, iPos.x - 1, iPos.y - 1, iPos.z - 1));

			// back right
			ao.y = vertexAO(
				isVoxelAt(voxelChunk, iPos.x + 1, iPos.y - 1, iPos.z),
				isVoxelAt(voxelChunk, iPos.x, iPos.y - 1, iPos.z - 1),
				isVoxelAt(voxelChunk, iPos.x + 1, iPos.y - 1, iPos.z - 1));

			generateFace(vertices, indices, nearCorner, forward, right, down, ao * 0.33f);
		}

		// Left Face
		if (!isVoxelAt(voxelChunk, iPos.x - 1, iPos.y, iPos.z)) {
			// top right
			ao.x = vertexAO(
				isVoxelAt(voxelChunk, iPos.x + 1, iPos.y, iPos.z),
				isVoxelAt(voxelChunk, iPos.x, iPos.y + 1, iPos.z),
				isVoxelAt(voxelChunk, iPos.x + 1, iPos.y + 1, iPos.z));

			// top left
			ao.y = vertexAO(
				isVoxelAt(voxelChunk, iPos.x - 1, iPos.y, iPos.z),
				isVoxelAt(voxelChunk, iPos.x, iPos.y + 1, iPos.z),
				isVoxelAt(voxelChunk, iPos.x - 1, iPos.y + 1, iPos.z));

			// bottom left
			ao.z = vertexAO(
				isVoxelAt(voxelChunk, iPos.x - 1, iPos.y, iPos.z),
				isVoxelAt(voxelChunk, iPos.x, iPos.y - 1, iPos.z),
				isVoxelAt(voxelChunk, iPos.x - 1, iPos.y - 1, iPos.z));

			// bottom right
			ao.w = vertexAO(
				isVoxelAt(voxelChunk, iPos.x + 1, iPos.y, iPos.z),
				isVoxelAt(voxelChunk, iPos.x, iPos.y - 1, iPos.z),
				isVoxelAt(voxelChunk, iPos.x + 1, iPos.y - 1, iPos.z));

			generateFace(vertices, indices, nearCorner, up, forward, left, glm::vec4(1.0f));
		}

		// Right Face
		if (!isVoxelAt(voxelChunk, iPos.x + 1, iPos.y, iPos.z)) {
			// front right
			ao.x = vertexAO(
				isVoxelAt(voxelChunk, iPos.x + 1, iPos.y, iPos.z),
				isVoxelAt(voxelChunk, iPos.x, iPos.y, iPos.z + 1),
				isVoxelAt(voxelChunk, iPos.x + 1, iPos.y, iPos.z + 1));

			// front left
			ao.y = vertexAO(
				isVoxelAt(voxelChunk, iPos.x - 1, iPos.y, iPos.z),
				isVoxelAt(voxelChunk, iPos.x, iPos.y, iPos.z + 1),
				isVoxelAt(voxelChunk, iPos.x - 1, iPos.y, iPos.z + 1));

			// back left
			ao.z = vertexAO(
				isVoxelAt(voxelChunk, iPos.x - 1, iPos.y, iPos.z),
				isVoxelAt(voxelChunk, iPos.x, iPos.y, iPos.z - 1),
				isVoxelAt(voxelChunk, iPos.x - 1, iPos.y, iPos.z - 1));

			// back right
			ao.w = vertexAO(
				isVoxelAt(voxelChunk, iPos.x + 1, iPos.y, iPos.z),
				isVoxelAt(voxelChunk, iPos.x, iPos.y, iPos.z - 1),
				isVoxelAt(voxelChunk, iPos.x + 1, iPos.y, iPos.z - 1));

			generateFace(vertices, indices, farCorner, backward, down, right, glm::vec4(1.0f));
		}

		// Front Face
		if (!isVoxelAt(voxelChunk, iPos.x, iPos.y, iPos.z - 1)) {
			// front right
			ao.x = vertexAO(
				isVoxelAt(voxelChunk, iPos.x + 1, iPos.y, iPos.z),
				isVoxelAt(voxelChunk, iPos.x, iPos.y, iPos.z + 1),
				isVoxelAt(voxelChunk, iPos.x + 1, iPos.y, iPos.z + 1));

			// front left
			ao.y = vertexAO(
				isVoxelAt(voxelChunk, iPos.x - 1, iPos.y, iPos.z),
				isVoxelAt(voxelChunk, iPos.x, iPos.y, iPos.z + 1),
				isVoxelAt(voxelChunk, iPos.x - 1, iPos.y, iPos.z + 1));

			// back left
			ao.z = vertexAO(
				isVoxelAt(voxelChunk, iPos.x - 1, iPos.y, iPos.z),
				isVoxelAt(voxelChunk, iPos.x, iPos.y, iPos.z - 1),
				isVoxelAt(voxelChunk, iPos.x - 1, iPos.y, iPos.z - 1));

			// back right
			ao.w = vertexAO(
				isVoxelAt(voxelChunk, iPos.x + 1, iPos.y, iPos.z),
				isVoxelAt(voxelChunk, iPos.x, iPos.y, iPos.z - 1),
				isVoxelAt(voxelChunk, iPos.x + 1, iPos.y, iPos.z - 1));

			generateFace(vertices, indices, nearCorner, right, up, forward, glm::vec4(1.0f));
		}

		// Back Face
		if (!isVoxelAt(voxelChunk, iPos.x, iPos.y, iPos.z + 1)) {
			// front right
			ao.x = vertexAO(
				isVoxelAt(voxelChunk, iPos.x + 1, iPos.y, iPos.z),
				isVoxelAt(voxelChunk, iPos.x, iPos.y, iPos.z + 1),
				isVoxelAt(voxelChunk, iPos.x + 1, iPos.y, iPos.z + 1));

			// front left
			ao.y = vertexAO(
				isVoxelAt(voxelChunk, iPos.x - 1, iPos.y, iPos.z),
				isVoxelAt(voxelChunk, iPos.x, iPos.y, iPos.z + 1),
				isVoxelAt(voxelChunk, iPos.x - 1, iPos.y, iPos.z + 1));

			// back left
			ao.z = vertexAO(
				isVoxelAt(voxelChunk, iPos.x - 1, iPos.y, iPos.z),
				isVoxelAt(voxelChunk, iPos.x, iPos.y, iPos.z - 1),
				isVoxelAt(voxelChunk, iPos.x - 1, iPos.y, iPos.z - 1));

			// back right
			ao.w = vertexAO(
				isVoxelAt(voxelChunk, iPos.x + 1, iPos.y, iPos.z),
				isVoxelAt(voxelChunk, iPos.x, iPos.y, iPos.z - 1),
				isVoxelAt(voxelChunk, iPos.x + 1, iPos.y, iPos.z - 1));

			generateFace(vertices, indices, farCorner, down, left, backward, glm::vec4(1.0f));
		}
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