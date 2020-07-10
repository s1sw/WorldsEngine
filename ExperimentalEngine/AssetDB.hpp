#pragma once
#include <cstdint>
#include <unordered_map>
#include <physfs.h>

enum class AssetType {
	Mesh,
	Texture,
	Material,
	Sound,
	Misc
};

typedef uint32_t AssetID;

struct MeshAsset {
	AssetID id;
	uint32_t vertexCount;
	uint32_t indexCount;
	bool useSmallIndices;

	template <class Archive>
	void serialize(Archive& archive) {
		archive(id, vertexCount, indexCount, useSmallIndices);
	}
};

struct TextureAsset {
	AssetID id;
	uint32_t width;
	uint32_t height;
	uint32_t mipLevels;
	bool isCompressed;

	template <class Archive>
	void serialize(Archive& archive) {
		archive(id, width, height, mipLevels, isCompressed);
	}
};

struct SoundAsset {
	AssetID id;
	uint32_t length;
	uint32_t channels;
	bool streamed;

	template <class Archive>
	void serialize(Archive& archive) {
		archive(id, length, channels, streamed);
	}
};

struct MaterialAsset {
	float roughness;
	float metallic;
	AssetID albedoTex;
	AssetID marTex; // metallic, ambient, roughness
};

struct ShaderAsset {
	AssetID id;
	// TODO: fill in things like parameters
};

class AssetDB {
public:
	AssetDB();
	TextureAsset getTexture(AssetID id);
	SoundAsset getSound(AssetID id);
	MeshAsset getMesh(AssetID id);
	MaterialAsset getMaterial(AssetID id);
	ShaderAsset getShader(AssetID id);
	PHYSFS_File* openDataFile(AssetID id);
	AssetID addAsset(std::string path);
	std::string getAssetPath(AssetID id) { return paths[id]; }
	std::string getAssetExtension(AssetID id) { return extensions[id]; }
	void save();

	template <class Archive>
	void serialize(Archive& archive) {
		archive(paths, textures, sounds, meshes, materials);
	}
private:
	AssetID currId;
	std::unordered_map<AssetID, std::string> paths;
	std::unordered_map<std::string, AssetID> ids;
	std::unordered_map<AssetID, std::string> extensions;
	std::unordered_map<AssetID, TextureAsset> textures;
	std::unordered_map<AssetID, SoundAsset> sounds;
	std::unordered_map<AssetID, MeshAsset> meshes;
	std::unordered_map<AssetID, MaterialAsset> materials;
};