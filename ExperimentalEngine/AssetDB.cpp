#include "PCH.hpp"
#include "AssetDB.hpp"
#include <filesystem>
#include <iostream>

AssetDB::AssetDB() : currId(0) {

}

PHYSFS_File* AssetDB::openAssetFileRead(AssetID id) {
	return PHYSFS_openRead(paths.at(id).c_str());
}

AssetID AssetDB::addAsset(std::string path) {
	if (PHYSFS_exists(path.c_str()) == 0) {
		std::cout << "Tried adding nonexistent asset\n";
		return ~0u;
	}

	AssetID id = currId++;

	paths.insert({ id, path });

	// Figure out the file extension
	auto ext = std::filesystem::path(path).extension().u8string();
	std::cout << "Added asset with extension " << ext << "\n";
	if (ext == ".png") {
		std::cout << "Texture\n";
	}
	extensions.insert({ id, ext });
	ids.insert({ path, id });

	return id;
}
