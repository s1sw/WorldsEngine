#pragma once
#include <noise/noise.h>
#include "VoxelChunk.hpp"
#include <glm/glm.hpp>

class ChunkGenerator {
public:
	ChunkGenerator() : perlin() {

	}

	void setSeed(int seed) {
		perlin.SetSeed(seed);
		groundLevelNoise.SetSeed(seed);
	}

	void setFrequency(double frequency) {
		perlin.SetFrequency(frequency);
	}

	void setOctaveCount(int octaves) {
		perlin.SetOctaveCount(octaves);
	}

	void setLacunarity(double lacunarity) {
		perlin.SetLacunarity(lacunarity);
	}

	void setPersistence(double persistence) {
		perlin.SetPersistence(persistence);
	}

	void fillChunk(VoxelChunk& chunk, glm::vec3 chunkOrigin) {
		//chunk.clear();
		// Block IDs:
		// 1 - stone
		// 2 - dirt
		// 3 - grass

		// First pass - fill in the basic terrain shape
		for (int x = 0; x < 15; x++)
			for (int y = 0; y < 15; y++)
				for (int z = 0; z < 15; z++) {
					int groundLevel = glm::floor(groundLevelNoise.GetValue((x + chunkOrigin.x) * 0.003, (z + chunkOrigin.z) * 0.003, 1.0) * 25.0);
					int globalY = y + chunkOrigin.y;

					if (globalY <= groundLevel){
						if (globalY < groundLevel - 3) {
							if (globalY > groundLevel - 50) {
								double val = perlin.GetValue((x + chunkOrigin.x) * 0.03125, (y + chunkOrigin.y - 25) * 0.03125, (z + chunkOrigin.z) * 0.03125);
								if (val > 0.25f)
									chunk.data[x][y][z] = 1;
								else
									chunk.data[x][y][z] = 0;
							} else {
								chunk.data[x][y][z] = 1;
							}
						}
						else {
							if (globalY >= groundLevel)
								chunk.data[x][y][z] = 3; // grass layer
							else
								chunk.data[x][y][z] = 2; // dirt
						}
						continue;
					} else {
						chunk.data[x][y][z] = 0;
					}
				}

		// Second pass - set grass block at top of each column
		/*for (int x = 0; x < 15; x++)
			for (int z = 0; z < 15; z++) {
				bool hasBlock = false;
				int maxY = 0;
				for (int y = 0; y < 15; y++) {
					if (chunk.data[x][y][z] == 2 && y > maxY) {
						hasBlock = true;
						maxY = y;
					}
				}

				if (hasBlock)
					chunk.data[x][maxY][z] = 3;
			}*/
	}
private:
	noise::module::Perlin perlin;
	noise::module::Perlin groundLevelNoise;
};