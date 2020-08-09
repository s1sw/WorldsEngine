#include "PCH.hpp"
#include "AssetDB.hpp"
#include <filesystem>
#include <iostream>
#include "Fatal.hpp"

const char* ASSET_DB_PATH = "assetDB.wdb";
const char ASSET_DB_MAGIC[] = {'W','A','D','B'};
const uint8_t ASSET_DB_VER = 1;

AssetDB::AssetDB() : currId(0) {

}

void AssetDB::load() {
	PHYSFS_File* dbFile = PHYSFS_openRead(ASSET_DB_PATH);

	char magicBuf[5];

	// Add a null byte to make it a valid null-terminated string just in case
	magicBuf[4] = 0;

	PHYSFS_readBytes(dbFile, magicBuf, 4);
	
	if (memcmp(magicBuf, ASSET_DB_MAGIC, 4) != 0) {
		fatalErr("AssetDB magic was invalid. Data is likely corrupted.");
	}

	uint8_t ver;
	PHYSFS_readBytes(dbFile, &ver, sizeof(ver));

	uint32_t idCount;
	PHYSFS_readULE32(dbFile, &idCount);

	ids.reserve(idCount);

	uint32_t maxId = 0;

	for (uint32_t i = 0; i < idCount; i++) {
		std::string path;
		
		uint16_t pathLen;
		PHYSFS_readULE16(dbFile, &pathLen);
		path.resize(pathLen);

		PHYSFS_readBytes(dbFile, path.data(), pathLen);

		uint32_t id;
		PHYSFS_readULE32(dbFile, &id);
		paths.insert({ id, path });
		ids.insert({ path, id });

		auto ext = std::filesystem::path(path).extension().string();
		extensions.insert({ id, ext });

		maxId = std::max(id, maxId);
	}

	currId = maxId + 1;

	PHYSFS_close(dbFile);
}

void AssetDB::save() {
	PHYSFS_File* dbFile = PHYSFS_openWrite(ASSET_DB_PATH);
	PHYSFS_writeBytes(dbFile, ASSET_DB_MAGIC, sizeof(ASSET_DB_MAGIC));
	PHYSFS_writeBytes(dbFile, &ASSET_DB_VER, sizeof(ASSET_DB_VER));
	PHYSFS_writeULE32(dbFile, ids.size());
	
	for (auto& p : ids) {
		PHYSFS_writeULE16(dbFile, p.first.size());
		PHYSFS_writeBytes(dbFile, p.first.data(), p.first.size());
		PHYSFS_writeULE32(dbFile, p.second);
	}

	PHYSFS_close(dbFile);
}

PHYSFS_File* AssetDB::openAssetFileRead(AssetID id) {
	return PHYSFS_openRead(paths.at(id).c_str());
}

PHYSFS_File* AssetDB::openAssetFileWrite(AssetID id) {
	return PHYSFS_openWrite(paths.at(id).c_str());
}

AssetID AssetDB::addAsset(std::string path) {
	if (PHYSFS_exists(path.c_str()) == 0) {
		std::cout << "Tried adding nonexistent asset\n";
		return ~0u;
	}

	AssetID id = currId++;

	paths.insert({ id, path });

	// Figure out the file extension
	auto ext = std::filesystem::path(path).extension().string();
	std::cout << "Added asset with extension " << ext << "\n";
	if (ext == ".png") {
		std::cout << "Texture\n";
	}
	extensions.insert({ id, ext });
	ids.insert({ path, id });

	return id;
}

AssetID AssetDB::createAsset(std::string path) {
	PHYSFS_close(PHYSFS_openWrite(path.c_str()));
	return addAsset(path);
}
