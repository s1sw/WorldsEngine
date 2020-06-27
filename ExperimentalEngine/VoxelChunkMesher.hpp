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
		return (3.0f - (side1 + side2 + corner)) * 0.33333f;
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
			glm::vec3 widthDir = left;
			glm::vec3 lengthDir = backward;

			// front right
			ao.x = vertexAO(
				isVoxelAt(voxelChunk, pos - widthDir + norm),
				isVoxelAt(voxelChunk, pos - lengthDir + norm),
				isVoxelAt(voxelChunk, pos - lengthDir - widthDir + norm)
			);

			// back right
			ao.y = vertexAO(
				isVoxelAt(voxelChunk, pos - widthDir + norm),
				isVoxelAt(voxelChunk, pos + lengthDir + norm),
				isVoxelAt(voxelChunk, pos - widthDir + lengthDir + norm)
			);

			// back left
			ao.z = vertexAO(
				isVoxelAt(voxelChunk, pos + widthDir + norm),
				isVoxelAt(voxelChunk, pos + lengthDir + norm),
				isVoxelAt(voxelChunk, pos + widthDir + lengthDir + norm)
			);

			// front left
			ao.w = vertexAO(
				isVoxelAt(voxelChunk, pos + widthDir + norm),
				isVoxelAt(voxelChunk, pos - lengthDir + norm),
				isVoxelAt(voxelChunk, pos - lengthDir + widthDir + norm)
			);

			

			generateFace(vertices, indices, farCorner, left, backward, up, ao);
		}

		// Bottom Face
		if (!isVoxelAt(voxelChunk, iPos.x, iPos.y - 1, iPos.z)) {
			//// front right
			//ao.z = vertexAO(
			//	isVoxelAt(voxelChunk, iPos.x + 1, iPos.y - 1, iPos.z),
			//	isVoxelAt(voxelChunk, iPos.x, iPos.y - 1, iPos.z + 1),
			//	isVoxelAt(voxelChunk, iPos.x + 1, iPos.y - 1, iPos.z + 1));

			//// front left
			//ao.w = vertexAO(
			//	isVoxelAt(voxelChunk, iPos.x - 1, iPos.y - 1, iPos.z),
			//	isVoxelAt(voxelChunk, iPos.x, iPos.y - 1, iPos.z + 1),
			//	isVoxelAt(voxelChunk, iPos.x - 1, iPos.y - 1, iPos.z + 1));

			//// back left
			//ao.x = vertexAO(
			//	isVoxelAt(voxelChunk, iPos.x - 1, iPos.y - 1, iPos.z),
			//	isVoxelAt(voxelChunk, iPos.x, iPos.y - 1, iPos.z - 1),
			//	isVoxelAt(voxelChunk, iPos.x - 1, iPos.y - 1, iPos.z - 1));

			//// back right
			//ao.y = vertexAO(
			//	isVoxelAt(voxelChunk, iPos.x + 1, iPos.y - 1, iPos.z),
			//	isVoxelAt(voxelChunk, iPos.x, iPos.y - 1, iPos.z - 1),
			//	isVoxelAt(voxelChunk, iPos.x + 1, iPos.y - 1, iPos.z - 1));

			glm::vec3 norm = down;
			glm::vec3 widthDir = forward;
			glm::vec3 lengthDir = right;

			// front right
			ao.x = vertexAO(
				isVoxelAt(voxelChunk, pos - widthDir + norm),
				isVoxelAt(voxelChunk, pos - lengthDir + norm),
				isVoxelAt(voxelChunk, pos - lengthDir - widthDir + norm)
			);

			// front left
			ao.w = vertexAO(
				isVoxelAt(voxelChunk, pos + widthDir + norm),
				isVoxelAt(voxelChunk, pos - lengthDir + norm),
				isVoxelAt(voxelChunk, pos - lengthDir + widthDir + norm)
			);

			// back left
			ao.z = vertexAO(
				isVoxelAt(voxelChunk, pos + widthDir + norm),
				isVoxelAt(voxelChunk, pos + lengthDir + norm),
				isVoxelAt(voxelChunk, pos + widthDir + lengthDir + norm)
			);

			// back right
			ao.y = vertexAO(
				isVoxelAt(voxelChunk, pos - widthDir + norm),
				isVoxelAt(voxelChunk, pos + lengthDir + norm),
				isVoxelAt(voxelChunk, pos - widthDir + lengthDir + norm)
			);

			generateFace(vertices, indices, nearCorner, forward, right, down, ao);
		}

		// Left Face
		if (!isVoxelAt(voxelChunk, iPos.x - 1, iPos.y, iPos.z)) {
			glm::vec3 norm = left;
			glm::vec3 widthDir = up;
			glm::vec3 lengthDir = forward;

			// front right
			ao.x = vertexAO(
				isVoxelAt(voxelChunk, pos - widthDir + norm),
				isVoxelAt(voxelChunk, pos - lengthDir + norm),
				isVoxelAt(voxelChunk, pos - lengthDir - widthDir + norm)
			);

			// front left
			ao.w = vertexAO(
				isVoxelAt(voxelChunk, pos + widthDir + norm),
				isVoxelAt(voxelChunk, pos - lengthDir + norm),
				isVoxelAt(voxelChunk, pos - lengthDir + widthDir + norm)
			);

			// back left
			ao.z = vertexAO(
				isVoxelAt(voxelChunk, pos + widthDir + norm),
				isVoxelAt(voxelChunk, pos + lengthDir + norm),
				isVoxelAt(voxelChunk, pos + widthDir + lengthDir + norm)
			);

			// back right
			ao.y = vertexAO(
				isVoxelAt(voxelChunk, pos - widthDir + norm),
				isVoxelAt(voxelChunk, pos + lengthDir + norm),
				isVoxelAt(voxelChunk, pos - widthDir + lengthDir + norm)
			);

			generateFace(vertices, indices, nearCorner, up, forward, left, ao);
		}

		// Right Face
		if (!isVoxelAt(voxelChunk, iPos.x + 1, iPos.y, iPos.z)) {
			glm::vec3 norm = right;
			glm::vec3 widthDir = backward;
			glm::vec3 lengthDir = down;

			// front right
			ao.x = vertexAO(
				isVoxelAt(voxelChunk, pos - widthDir + norm),
				isVoxelAt(voxelChunk, pos - lengthDir + norm),
				isVoxelAt(voxelChunk, pos - lengthDir - widthDir + norm)
			);

			// front left
			ao.w = vertexAO(
				isVoxelAt(voxelChunk, pos + widthDir + norm),
				isVoxelAt(voxelChunk, pos - lengthDir + norm),
				isVoxelAt(voxelChunk, pos - lengthDir + widthDir + norm)
			);

			// back left
			ao.z = vertexAO(
				isVoxelAt(voxelChunk, pos + widthDir + norm),
				isVoxelAt(voxelChunk, pos + lengthDir + norm),
				isVoxelAt(voxelChunk, pos + widthDir + lengthDir + norm)
			);

			// back right
			ao.y = vertexAO(
				isVoxelAt(voxelChunk, pos - widthDir + norm),
				isVoxelAt(voxelChunk, pos + lengthDir + norm),
				isVoxelAt(voxelChunk, pos - widthDir + lengthDir + norm)
			);

			generateFace(vertices, indices, farCorner, backward, down, right, ao);
		}

		// Front Face
		if (!isVoxelAt(voxelChunk, iPos.x, iPos.y, iPos.z - 1)) {
			glm::vec3 norm = forward;
			glm::vec3 widthDir = right;
			glm::vec3 lengthDir = up;

			// top right
			ao.x = vertexAO(
				isVoxelAt(voxelChunk, pos - widthDir - norm),
				isVoxelAt(voxelChunk, pos - lengthDir - norm),
				isVoxelAt(voxelChunk, pos - lengthDir - widthDir - norm)
			);

			// top left
			ao.y = vertexAO(
				isVoxelAt(voxelChunk, pos - widthDir - norm),
				isVoxelAt(voxelChunk, pos + lengthDir - norm),
				isVoxelAt(voxelChunk, pos + lengthDir - widthDir - norm)
			);

			// bottom left
			ao.z = vertexAO(
				isVoxelAt(voxelChunk, pos + widthDir - norm),
				isVoxelAt(voxelChunk, pos + lengthDir - norm),
				isVoxelAt(voxelChunk, pos + lengthDir + widthDir - norm)
			);

			// bottom right
			ao.w = vertexAO(
				isVoxelAt(voxelChunk, pos + widthDir - norm),
				isVoxelAt(voxelChunk, pos - lengthDir - norm),
				isVoxelAt(voxelChunk, pos - lengthDir + widthDir - norm)
			);

			generateFace(vertices, indices, nearCorner, right, up, forward, ao * 0.3f);
		}

		// Back Face
		if (!isVoxelAt(voxelChunk, iPos.x, iPos.y, iPos.z + 1)) {
			glm::vec3 norm = backward;
			glm::vec3 widthDir = down;
			glm::vec3 lengthDir = left;

			// top right
			ao.x = vertexAO(
				isVoxelAt(voxelChunk, pos + right - norm),
				isVoxelAt(voxelChunk, pos + up - norm),
				isVoxelAt(voxelChunk, pos + right + up - norm)
			);

			// top left
			ao.y = vertexAO(
				isVoxelAt(voxelChunk, pos + left - norm),
				isVoxelAt(voxelChunk, pos + up - norm),
				isVoxelAt(voxelChunk, pos + left + up - norm)
			);

			// bottom left
			ao.z = vertexAO(
				isVoxelAt(voxelChunk, pos + left - norm),
				isVoxelAt(voxelChunk, pos + down - norm),
				isVoxelAt(voxelChunk, pos + left + down - norm)
			);

			// bottom right
			ao.w = vertexAO(
				isVoxelAt(voxelChunk, pos + right - norm),
				isVoxelAt(voxelChunk, pos + down - norm),
				isVoxelAt(voxelChunk, pos + right + down - norm)
			);

			// vertex order is top right, top left, bottom left, bottom right

			generateFace(vertices, indices, farCorner, down, left, backward, ao * 0.33f);
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