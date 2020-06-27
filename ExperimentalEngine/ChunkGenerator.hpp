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
		chunk.clear();
		for (int x = 0; x < 15; x++)
			for (int y = 0; y < 15; y++)
				for (int z = 0; z < 15; z++) {
					if ((y + chunkOrigin.y) > 70)
						continue;

					if (y + chunkOrigin.y < groundLevelNoise.GetValue((x + chunkOrigin.x) * 0.03125, (z + chunkOrigin.z) * 0.03125, 1.0) * 10.0){
						chunk.data[x][y][z] = 1;
						continue;
					}

					double val = perlin.GetValue((x + chunkOrigin.x) * 0.03125, (y + chunkOrigin.y) * 0.03125, (z + chunkOrigin.z) * 0.03125);
					if (val > 0.25f)
						chunk.data[x][y][z] = 1;
				}
	}
private:
	noise::module::Perlin perlin;
	noise::module::Perlin groundLevelNoise;
};