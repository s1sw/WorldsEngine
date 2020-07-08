#pragma once
#include "Engine.hpp"
#include "VoxelChunk.hpp"

enum class VoxelFace {
	Top,
	Bottom,
	Left,
	Right,
	Front,
	Back,
	Count
};

struct VoxelTextureData {
	glm::vec2 offset;
	bool useFaceOffsets;
	glm::vec2 faceOffsets[(int)VoxelFace::Count];
};

const VoxelTextureData texData[4] = {
	// 0 is air
	{},
	// 1 is stone
	{ glm::vec2{1.0f, 0.0f}, false },
	// 2 is dirt
	{ glm::vec2{2.0f, 0.0f}, false },
	// 3 is grass
	{ glm::vec2{3.0f, 0.0f}, true, {glm::vec2{0.0f, 0.0f}, glm::vec2{2.0f, 0.0f}, {3.0f, 0.0f}, {3.0f, 0.0f}, {3.0f, 0.0f}, {3.0f, 0.0f}} }
};

class VoxelChunkMesher {
	uint32_t packVec3(glm::vec3 vec) {
		uint32_t result = 0;
		result |= ((uint32_t)vec.x) << 0;
		result |= ((uint32_t)vec.y) << 8;
		result |= ((uint32_t)vec.z) << 16;
		return result;
	}

	uint32_t packVec2(glm::vec2 vec) {
		uint32_t result = 0;
		result |= ((uint32_t)vec.x) << 0;
		result |= ((uint32_t)vec.y) << 16;
		return result;
	}

	void generateFace(std::vector<ChunkVertex>& vertices, std::vector<uint32_t>& indices, glm::vec3 offset, glm::vec3 widthDir, glm::vec3 lengthDir, uint8_t normIdx, uint8_t cornerIndices[4], glm::vec4 vertAO, glm::vec2 atlasPos) {
		// indices are always the same
		uint32_t idxOffset = (uint32_t)vertices.size();
		vertices.push_back(ChunkVertex{ packVec3(offset), ((uint32_t)normIdx) | (uint32_t)cornerIndices[0] << 8, vertAO.x, packVec2(atlasPos) });
		vertices.push_back(ChunkVertex{ packVec3(offset + lengthDir), ((uint32_t)normIdx) | (uint32_t)cornerIndices[1] << 8, vertAO.y, packVec2(atlasPos) });
		vertices.push_back(ChunkVertex{ packVec3(offset + lengthDir + widthDir), ((uint32_t)normIdx) | (uint32_t)cornerIndices[2] << 8, vertAO.z, packVec2(atlasPos) });
		vertices.push_back(ChunkVertex{ packVec3(offset + widthDir), ((uint32_t)normIdx) | (uint32_t)cornerIndices[3] << 8, vertAO.w, packVec2(atlasPos) });

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
		if (side1 && side2) return 0.1f;

		// bools are just spicy ints
		return glm::max((3.0f - (side1 + side2 + corner)) * 0.33333f, 0.1f);
	}

	void genBlockFaces(VoxelChunk& voxelChunk, glm::vec3 pos, std::vector<ChunkVertex>& vertices, std::vector<uint32_t>& indices) {
		//if (!isVoxelAt(voxelChunk, pos.x, pos.y, pos.z)) return;
		if (voxelChunk.data[(int)pos.x][(int)pos.y][(int)pos.z] == 0) return;
		char thisVoxel = voxelChunk.data[(int)pos.x][(int)pos.y][(int)pos.z];
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

			glm::vec2 uvs[4] = {
				{0.0f, 0.0625f},
				{0.0625f, 0.0625f},
				{0.0625f, 0.0},
				{0.0f, 0.0f}
			};

			uint8_t uvIndices[4] = { 0,1,2,3 };

			auto& voxTexData = texData[thisVoxel];

			glm::vec2 offset = voxTexData.useFaceOffsets ? voxTexData.faceOffsets[(int)VoxelFace::Top] : voxTexData.offset;

			generateFace(vertices, indices, farCorner, left, backward, 0, uvIndices, ao, offset);
		}

		// Bottom Face
		if (!isVoxelAt(voxelChunk, iPos.x, iPos.y - 1, iPos.z)) {
			glm::vec3 norm = down;
			glm::vec3 widthDir = forward;
			glm::vec3 lengthDir = right;

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

			glm::vec2 uvs[4] = {
				{0.0f, 0.0625f},
				{0.0625f, 0.0625f},
				{0.0625f, 0.0},
				{0.0f, 0.0f}
			};

			uint8_t uvIndices[4] = { 0,1,2,3 };

			auto& voxTexData = texData[thisVoxel];

			glm::vec2 offset = voxTexData.useFaceOffsets ? voxTexData.faceOffsets[(int)VoxelFace::Bottom] : voxTexData.offset;

			generateFace(vertices, indices, nearCorner, forward, right, 1, uvIndices, ao, offset);
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

			glm::vec2 uvs[4] = {
				{0.0f, 0.0625f},
				{0.0625f, 0.0625f},
				{0.0625f, 0.0f},
				{0.0f, 0.0f}
			};

			uint8_t uvIndices[4] = { 0,1,2,3 };

			auto& voxTexData = texData[thisVoxel];

			glm::vec2 offset = voxTexData.useFaceOffsets ? voxTexData.faceOffsets[(int)VoxelFace::Left] : voxTexData.offset;

			generateFace(vertices, indices, nearCorner, up, forward, 5, uvIndices, ao, offset);
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

			glm::vec2 uvs[4] = {
				{0.0625f, 0.0f},
				{0.0625f, 0.0625f},
				{0.0f, 0.0625f},
				{0.0f, 0.0f}
			};

			uint8_t uvIndices[4] = { 2, 1, 0, 3 };


			auto& voxTexData = texData[thisVoxel];

			glm::vec2 offset = voxTexData.useFaceOffsets ? voxTexData.faceOffsets[(int)VoxelFace::Right] : voxTexData.offset;

			generateFace(vertices, indices, farCorner, backward, down, 4, uvIndices, ao, offset);
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

			glm::vec2 uvs[4] = {
				{0.0f, 0.0625f},
				{0.0f, 0.0f},
				{0.0625f, 0.0f},
				{0.0625f, 0.0625f}
			};

			uint8_t uvIndices[4] = { 0, 3, 2, 1 };

			auto& voxTexData = texData[thisVoxel];

			glm::vec2 offset = voxTexData.useFaceOffsets ? voxTexData.faceOffsets[(int)VoxelFace::Front] : voxTexData.offset;

			generateFace(vertices, indices, nearCorner, right, up, 2, uvIndices, ao, offset);
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

			glm::vec2 uvs[4] = {
				{0.0f, 0.0f},
				{0.0625f, 0.0f},
				{0.0625f, 0.0625f},
				{0.0f, 0.0625f}
			};

			uint8_t uvIndices[4] = { 3, 2, 1, 0 };

			auto& voxTexData = texData[thisVoxel];

			glm::vec2 offset = voxTexData.useFaceOffsets ? voxTexData.faceOffsets[(int)VoxelFace::Back] : voxTexData.offset;

			// vertex order is top right, top left, bottom left, bottom right

			generateFace(vertices, indices, farCorner, down, left, 3, uvIndices, ao, offset);
		}
	}

public:
	void updateVoxelChunk(VoxelChunk& voxelChunk, ChunkRenderObject& procObj) {
		if (!voxelChunk.dirty) return;

		std::vector<ChunkVertex> vertices;
		std::vector<uint32_t> indices;

		vertices.reserve(6*4*16*16);
		indices.reserve(6 * 16 * 16 * 3 * 2);

		for (int x = 0; x < 16; x++)
			for (int y = 0; y < 16; y++)
				for (int z = 0; z < 16; z++) {
					glm::vec3 pos(x, y, z);
					genBlockFaces(voxelChunk, pos, vertices, indices);
				}
		vertices.shrink_to_fit();
		indices.shrink_to_fit();
		procObj.vertices = std::move(vertices);
		procObj.indices = std::move(indices);
		procObj.uploaded = false;
		procObj.readyForUpload = true;
	}
};