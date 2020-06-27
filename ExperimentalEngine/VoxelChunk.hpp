#pragma once

struct VoxelChunk {
	VoxelChunk() {
		clear();
	}

	void clear() {
		memset(data, 0, sizeof(char) * 16 * 16 * 16);
	}
	char data[16][16][16];
	bool dirty;
};