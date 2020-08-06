#pragma once
#include <cstdint>
#include <unordered_map>
#include <physfs.h>

typedef uint32_t AssetID;

class AssetDB {
public:
	AssetDB();
	void load();
	void save();
	PHYSFS_File* openAssetFileRead(AssetID id);
	PHYSFS_File* openAssetFileWrite(AssetID id);

	// There's basically no need to use this ever.
	// In 99% of casees addOrGetExisting will be the better choice
	// in case of the asset already existing in the database.
	AssetID addAsset(std::string path);

	AssetID createAsset(std::string path);
	std::string getAssetPath(AssetID id) { return paths[id]; }
	std::string getAssetExtension(AssetID id) { return extensions[id]; }
	bool hasId(std::string path) { return ids.find(path) != ids.end(); }
	AssetID getExistingID(std::string path) { return ids.at(path); }
	AssetID addOrGetExisting(std::string path) { return hasId(path) ? getExistingID(path) : addAsset(path); }
private:
	AssetID currId;
	std::unordered_map<AssetID, std::string> paths;
	std::unordered_map<std::string, AssetID> ids;
	std::unordered_map<AssetID, std::string> extensions;
};